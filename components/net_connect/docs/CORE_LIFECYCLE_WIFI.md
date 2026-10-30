# Core lifecycle, walked through with Wi-Fi STA

This is the end-to-end path for **Flows 1–7** in [IMPLEMENTATION_FLOWS.md](./IMPLEMENTATION_FLOWS.md), using Wi-Fi STA as the concrete driver. The same core (`net_iface.c`) is shared by Ethernet, Thread, and PPP. Only the `net_iface_ops_t` callbacks differ.

Sources:

- `include/net_iface_priv.h` — handle, ops table, sticky event-group bits
- `net_iface.c` — registry, state machine, waits, notifications
- `wifi_connect.c` — `g_net_iface_wifi_sta_ops` and the ESP-IDF Wi-Fi calls

Line numbers below match those files as of this write-up.

---

## Call sequence

The application calls Wi-Fi to create the station and to store credentials. Start, connect, wait, disconnect, stop, and destroy go to `net_iface`, which calls the Wi-Fi ops. Wi-Fi drives ESP-IDF and reports back with `net_iface_notify_*()`. Association and IP events arrive on the default event loop.

Happy path: `INIT` → `STARTING` → `STARTED` → `CONNECTING` → `CONNECTED` → `READY`. `net_iface_start()` returns while the startup task is still bringing the radio up.

### Flows 1–4: create through ready

```mermaid
sequenceDiagram
    participant App as User Application
    participant WiFi as Wi-Fi (wifi_connect.c)
    participant Core as net_iface Core
    participant Task as Startup Task
    participant IDF as ESP-IDF Wi-Fi / netif
    participant Loop as Default Event Loop

    rect 1. Create handle (UNINIT -> INIT)
    App->>WiFi: net_wifi_sta_create()
    WiFi->>WiFi: Read Kconfig defaults, apply params
    WiFi->>Core: net_iface_init_with_ops(wifi_sta_ops)
    Core->>Core: Allocate handle, state = INIT (ops->init is NULL)
    opt Event loop exists and global handlers not registered yet
        Core->>Loop: Register (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return ESP_OK
    end
    Core-->>WiFi: Return handle
    WiFi-->>App: Return handle (ESP_OK)
    end

    rect 2. Start interface (INIT -> STARTING)
    App->>Core: net_iface_start()
    opt Global handlers not registered in step 1 (event loop created after handle)
        Core->>Loop: Register (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return ESP_OK
    end
    Core->>Core: Clear liveness and STARTUP_DONE bits, state = STARTING
    Core->>Task: xTaskCreate(iface_startup_task)
    Core-->>App: Return ESP_OK immediately
    end

    rect 3. Driver start (STARTING -> STARTED)
    Task->>WiFi: ops->start() (wifi_ops_start)
    WiFi->>IDF: esp_wifi_init() and esp_netif_create_wifi(STA)
    IDF-->>WiFi: Return netif
    WiFi->>IDF: esp_wifi_set_default_wifi_sta_handlers()
    IDF->>Loop: Register default STA netif handlers (WIFI_EVENT STA_START/STOP/CONNECTED/DISCONNECTED, IP_EVENT_STA_GOT_IP)
    Loop-->>IDF: Return ESP_OK
    IDF-->>WiFi: Return ESP_OK
    WiFi->>IDF: RAM storage, STA mode, esp_wifi_start()
    IDF-->>WiFi: Return ESP_OK
    WiFi->>Loop: Register (WIFI_EVENT, WIFI_EVENT_STA_CONNECTED) -> on_wifi_sta_connected
    Loop-->>WiFi: Return ESP_OK
    WiFi->>Loop: Register (WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED) -> on_wifi_sta_disconnected
    Loop-->>WiFi: Return ESP_OK
    WiFi->>Core: net_iface_notify_started(netif)
    Core->>Core: Save netif, state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_STARTED
    Loop-->>Core: Queued
    Core-->>WiFi: Return from net_iface_notify_started()
    WiFi-->>Task: Return ESP_OK from wifi_ops_start()
    end

    rect 4. Auto-connect (STARTED -> CONNECTING)
    Note over Task,Core: auto_connect_on_start is true and stop was not requested
    Task->>Core: net_iface_connect()
    Core->>Core: Clear READY/FAILED bits, state = CONNECTING
    Core->>WiFi: ops->connect() (wifi_ops_connect)
    WiFi->>IDF: esp_wifi_set_config() and esp_wifi_connect()
    IDF-->>WiFi: Return ESP_OK
    WiFi-->>Core: Return ESP_OK from wifi_ops_connect()
    Core-->>Task: Return ESP_OK from net_iface_connect()
    Task->>Core: Save stack HWM, set STARTUP_DONE
    Core-->>Task: Done
    Task->>Task: vTaskDelete(NULL)
    end

    rect 5. AP associated (CONNECTING -> CONNECTED)
    IDF->>Loop: Post WIFI_EVENT_STA_CONNECTED
    Loop->>IDF: Default STA handler starts DHCP client on netif
    IDF-->>Loop: Return
    Loop->>WiFi: on_wifi_sta_connected()
    WiFi->>Core: net_iface_notify_connected()
    Core->>Core: state = CONNECTED, retry_num = 0
    opt IPv6 enabled
        Core->>IDF: esp_netif_create_ip6_linklocal()
        IDF-->>Core: Return
    end
    Core->>Loop: Post NET_IFACE_EVENT_CONNECTED
    Loop-->>Core: Queued
    Core-->>WiFi: Return from net_iface_notify_connected()
    WiFi-->>Loop: Return from on_wifi_sta_connected()
    end

    rect 6. Address acquired (CONNECTED -> READY)
    IDF->>Loop: Post IP_EVENT_STA_GOT_IP (and IP_EVENT_GOT_IP6 if required)
    Loop->>Core: on_global_ip_event()
    Core->>Core: Set IPV4 / IPV6 bit, check is_ready_satisfied_locked()
    Core->>Core: net_iface_notify_ready() -> state = READY, set READY bit, pulse global wait
    Core->>Loop: Post NET_IFACE_EVENT_READY
    Loop-->>Core: Queued
    Core-->>Loop: Return from on_global_ip_event()
    end

    rect 7. Wait for readiness (READY)
    App->>Core: net_iface_wait(timeout)
    Core-->>App: READY bit is set -> return ESP_OK
    end
```

### Flow 7: disconnect, stop, and destroy

Disconnect leaves the radio and netif up. Stop tears them down and returns the handle to `INIT`, which is the only state `net_iface_deinit()` accepts.

```mermaid
sequenceDiagram
    participant App as User Application
    participant WiFi as Wi-Fi (wifi_connect.c)
    participant Core as net_iface Core
    participant IDF as ESP-IDF Wi-Fi / netif
    participant Loop as Default Event Loop

    rect 1. Disconnect (READY/CONNECTED -> STARTED)
    App->>Core: net_iface_disconnect()
    Core->>WiFi: ops->disconnect() (wifi_ops_disconnect)
    WiFi->>WiFi: s_wifi_ignore_disconnect = true
    WiFi->>IDF: esp_wifi_disconnect()
    IDF-->>WiFi: Return ESP_OK
    WiFi-->>Core: Return ESP_OK from wifi_ops_disconnect()
    Core->>Core: net_iface_notify_disconnected() -> state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_DISCONNECTED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK (radio and netif stay up)
    end

    rect 2. Stop driver (STARTED -> STOPPING -> INIT)
    App->>Core: net_iface_stop()
    Core->>Core: state = STOPPING (wait for STARTUP_DONE if task is running)
    Core->>WiFi: ops->disconnect() (wifi_ops_disconnect)
    WiFi->>IDF: esp_wifi_disconnect()
    IDF-->>WiFi: Return ESP_OK
    WiFi-->>Core: Return ESP_OK from wifi_ops_disconnect()
    Core->>WiFi: ops->stop() (wifi_ops_stop)
    WiFi->>Loop: Unregister (WIFI_EVENT, WIFI_EVENT_STA_CONNECTED) -> on_wifi_sta_connected
    Loop-->>WiFi: Return ESP_OK
    WiFi->>Loop: Unregister (WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED) -> on_wifi_sta_disconnected
    Loop-->>WiFi: Return ESP_OK
    WiFi->>IDF: esp_wifi_stop(), esp_wifi_deinit() if we inited
    IDF-->>WiFi: Return ESP_OK
    WiFi->>IDF: esp_wifi_clear_default_wifi_driver_and_handlers(), esp_netif_destroy()
    IDF->>Loop: Unregister default STA netif handlers
    Loop-->>IDF: Return ESP_OK
    IDF-->>WiFi: Return
    WiFi-->>Core: Return ESP_OK from wifi_ops_stop()
    Core->>Core: net_iface_notify_stopped() -> state = INIT, set STOPPED bit
    Core->>Loop: Post NET_IFACE_EVENT_STOPPED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK
    end

    rect 3. Destroy handle (INIT -> UNINIT)
    App->>Core: net_iface_deinit()
    Core->>Core: Verify state == INIT, unlink from registry
    opt Last interface removed from registry
        Core->>Loop: Unregister (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return
        Core->>Loop: Unregister (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return
    end
    Core->>WiFi: ops->deinit() (wifi_ops_deinit)
    WiFi-->>Core: Return ESP_OK from wifi_ops_deinit()
    Core->>Core: Free event group, mutex, and handle (state = UNINIT)
    Core-->>App: Return ESP_OK
    end
```

### Flows 5–6 and stop during startup

Empty SSID at create, address loss, join retries, and a stop that arrives while `wifi_ops_start()` is still running.

```mermaid
sequenceDiagram
    participant App as User Application
    participant WiFi as Wi-Fi (wifi_connect.c)
    participant Core as net_iface Core
    participant Task as Startup Task
    participant IDF as ESP-IDF Wi-Fi / netif
    participant Loop as Default Event Loop

    rect Flow 5 - Late credentials (INIT -> STARTED -> CONNECTING)
    App->>WiFi: 1. net_wifi_sta_create() with empty SSID
    WiFi->>Core: net_iface_init_with_ops() (auto_connect_on_start = false)
    Core-->>WiFi: Return handle
    WiFi-->>App: Return handle (ESP_OK)
    App->>Core: 2. net_iface_start()
    Core->>Task: xTaskCreate(iface_startup_task)
    Core-->>App: Return ESP_OK
    Task->>WiFi: ops->start() (wifi_ops_start)
    WiFi->>Core: net_iface_notify_started(netif) -> state = STARTED
    Core-->>WiFi: Return
    WiFi-->>Task: Return ESP_OK from wifi_ops_start()
    Note over Task,Core: auto_connect_on_start is false, connect is skipped
    Task->>Core: Set STARTUP_DONE
    Core-->>Task: Done
    Task->>Task: vTaskDelete(NULL)
    App->>WiFi: 3. net_wifi_sta_set_credentials(ssid, password)
    WiFi->>WiFi: Copy credentials into h->cfg.wifi_sta
    WiFi-->>App: Return ESP_OK
    App->>Core: 4. net_iface_connect()
    Core->>WiFi: ops->connect() (wifi_ops_connect)
    WiFi->>IDF: esp_wifi_set_config() and esp_wifi_connect()
    IDF-->>WiFi: Return ESP_OK
    WiFi-->>Core: Return ESP_OK from wifi_ops_connect()
    Core-->>App: Return ESP_OK from net_iface_connect()
    end

    rect Flow 6a - Address lost (READY -> CONNECTING)
    IDF->>Loop: 5. Post IP_EVENT_STA_LOST_IP
    Loop->>Core: on_global_ip_event()
    Core->>Core: net_iface_notify_lost_ip() -> clear READY and IPV4 bits, state = CONNECTING
    Core->>Loop: Post NET_IFACE_EVENT_LOST_IP
    Loop-->>Core: Queued
    Core-->>Loop: Return from on_global_ip_event()
    end

    rect Flow 6b - AP dropped (CONNECTING/READY -> FAILED)
    IDF->>Loop: 6. Post WIFI_EVENT_STA_DISCONNECTED
    Loop->>WiFi: on_wifi_sta_disconnected()
    alt retry_num <= max_retry
        WiFi->>IDF: esp_wifi_connect()
        IDF-->>WiFi: Return ESP_OK
        WiFi-->>Loop: Return from on_wifi_sta_disconnected()
    else retry_num > max_retry
        WiFi->>Core: net_iface_notify_failed(ESP_FAIL)
        Core->>Core: state = FAILED, set FAILED bit, wake waiters
        Core->>Loop: Post NET_IFACE_EVENT_FAILED
        Loop-->>Core: Queued
        Core-->>WiFi: Return from net_iface_notify_failed()
        WiFi-->>Loop: Return from on_wifi_sta_disconnected()
    end
    end

    rect Flow 7 - Stop during startup (STARTING -> STOPPING -> INIT)
    Note over App,Task: 7a. App calls stop() while Startup Task is still inside wifi_ops_start()
    Task->>WiFi: ops->start() (wifi_ops_start) running
    App->>Core: net_iface_stop()
    Core->>Core: cancel_requested = true, state = STOPPING
    Note over App,Core: App task sleeps on STARTUP_DONE bit (up to 10 s)

    Note over WiFi,Task: 7b. Startup Task finishes wifi_ops_start(), sees cancel_requested, and exits
    WiFi->>Core: net_iface_notify_started(netif)
    Core->>Core: State is STOPPING -> save h->netif only (do not enter STARTED)
    Core-->>WiFi: Return from net_iface_notify_started()
    WiFi-->>Task: Return ESP_OK from wifi_ops_start()
    Task->>Core: Check cancel_requested == true -> skip net_iface_connect()
    Task->>Core: Set STARTUP_DONE bit (wakes App task in net_iface_stop)
    Task->>Task: vTaskDelete(NULL)

    Note over App,Loop: 7c. App task wakes up inside net_iface_stop() and tears down the driver
    Core->>WiFi: ops->disconnect() (wifi_ops_disconnect)
    WiFi->>IDF: esp_wifi_disconnect()
    IDF-->>WiFi: Return ESP_OK
    WiFi-->>Core: Return ESP_OK
    Core->>WiFi: ops->stop() (wifi_ops_stop)
    WiFi->>Loop: Unregister STA_CONNECTED and STA_DISCONNECTED
    Loop-->>WiFi: Return ESP_OK
    WiFi->>IDF: esp_wifi_stop(), esp_wifi_deinit(), destroy h->netif
    IDF-->>WiFi: Return ESP_OK
    WiFi-->>Core: Return ESP_OK
    Core->>Core: net_iface_notify_stopped() -> state = INIT, set STOPPED bit
    Core->>Loop: Post NET_IFACE_EVENT_STOPPED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK from net_iface_stop()
    end
```

---

## The objects the core keeps

Every interface is one `struct net_iface`. The driver does not track readiness itself. It calls `net_iface_notify_*()`, and the core updates three things: the state, sticky bits in `h->state_events`, and a `NET_IFACE_EVENT` on the default event loop.

Sticky bits and the handle (`include/net_iface_priv.h`):

```c
/* Sticky event-group bits stored in iface->state_events */
#define NET_IFACE_BIT_READY        BIT0 /*!< Sticky bit: interface is in NET_IFACE_STATE_READY */
#define NET_IFACE_BIT_FAILED       BIT1 /*!< Sticky bit: interface is in NET_IFACE_STATE_FAILED */
#define NET_IFACE_BIT_STOPPED      BIT2 /*!< Sticky bit: interface is stopped / back in INIT */
#define NET_IFACE_BIT_IPV4         BIT3 /*!< IPv4 address acquired on this interface */
#define NET_IFACE_BIT_IPV6         BIT4 /*!< Preferred IPv6 address acquired on this interface */
#define NET_IFACE_BIT_OT_ATTACHED  BIT5 /*!< OpenThread attached event received */
#define NET_IFACE_BIT_STARTUP_DONE BIT6 /*!< Transient startup task has finished executing */
// ...
struct net_iface {
    const net_iface_ops_t *ops;              /*!< Driver callbacks */
    net_iface_config_t     cfg;              /*!< Copy of interface configuration */
    net_iface_state_t      state;            /*!< Current lifecycle state */
    esp_err_t              last_err;         /*!< Last error code when in FAILED state */
    esp_netif_t           *netif;            /*!< Underlying esp_netif instance */
    SemaphoreHandle_t      lock;             /*!< Per-interface mutex (acquired after registry mutex) */
    EventGroupHandle_t     state_events;     /*!< Sticky readiness/failure bits for wait APIs */
    TaskHandle_t           startup_task;     /*!< Handle of running startup/worker task, or NULL */
    uint32_t               startup_stack_hwm;/*!< Minimum free stack bytes left in the startup task */
    bool                   cancel_requested; /*!< Set by net_iface_stop() to abort in-flight startup */
    int                    retry_num;        /*!< Connection retry counter */
    void                  *drv;              /*!< Driver-private state pointer */
    struct net_iface      *next;             /*!< Next pointer in global registry list */
};
```

Wi-Fi fills the ops table in `wifi_connect.c` (lines 338–346):

```c
const net_iface_ops_t g_net_iface_wifi_sta_ops = {
    .name = "wifi_sta",
    .init = NULL,
    .deinit = wifi_ops_deinit,
    .start = wifi_ops_start,
    .stop = wifi_ops_stop,
    .connect = wifi_ops_connect,
    .disconnect = wifi_ops_disconnect,
};
```

`init` is `NULL` because creating a Wi-Fi handle does not touch the radio.

Two locks are used. `s_registry_mutex` guards the handle list. Each handle has `h->lock`. If both are needed, the list lock is taken first. Neither lock is held across a driver call (`start`, `stop`, `connect`, `disconnect`) or across `esp_event_post()`.

---

## Flow 1: Create the handle

Public call: `net_wifi_sta_create()` in `wifi_connect.c` (lines 348–399):

```c
esp_err_t net_wifi_sta_create(const net_wifi_sta_params_t *params, net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    net_iface_config_t cfg = { 0 };
    net_wifi_sta_config_t sta;
    populate_config_from_kconfig(&sta);
    fill_iface_config(&cfg, &sta);
    // ... overrides from params ...
    return net_iface_init_with_ops(&cfg, &g_net_iface_wifi_sta_ops, out_handle);
}
```

1. `populate_config_from_kconfig()` fills SSID, password, scan settings, `max_retry`, DHCP, and IPv6 from Kconfig. SSID and password are left empty when `CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN` is on.
2. `fill_iface_config()` builds `net_iface_config_t`:
   - `type = NET_IFACE_TYPE_WIFI_STA`
   - `desc = NET_CONNECT_NETIF_DESC_STA` (`"net_connect_netif_sta"`)
   - `route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO` (128)
   - `required` from `CONFIG_NET_CONNECT_WIFI_REQUIRED`
   - `auto_connect_on_start` is true only when the SSID is non-empty
3. A non-NULL `net_wifi_sta_params_t` overrides those fields. An empty SSID with `auto_connect_on_start == false` stays idle until credentials are stored later.
4. The function calls `net_iface_init_with_ops(&cfg, &g_net_iface_wifi_sta_ops, out_handle)`.

`net_iface_init_with_ops()` (`net_iface.c`, lines 724–781):

```c
esp_err_t net_iface_init_with_ops(const net_iface_config_t *cfg,
                                  const net_iface_ops_t *ops,
                                  net_iface_handle_t *out_handle)
{
    // ...
    esp_err_t err = ensure_registry_mutex();
    // ...
    net_iface_handle_t h = calloc(1, sizeof(*h));
    // ...
    h->lock = xSemaphoreCreateMutex();
    h->state_events = xEventGroupCreate();
    // ...
    h->cfg = *cfg;
    h->ops = ops;
    h->state = NET_IFACE_STATE_INIT;
    h->last_err = ESP_OK;
    // ...
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    h->next = s_iface_list;
    s_iface_list = h;
    xSemaphoreGive(s_registry_mutex);

    try_register_global_handlers();

    *out_handle = h;
    return ESP_OK;
}
```

1. `ensure_registry_mutex()` creates `s_registry_mutex` and `s_global_wait_events` the first time any handle is created. Later creates reuse them.
2. Allocates the handle, `h->lock`, and `h->state_events`. Copies the config, stores the ops pointer, and sets `h->state = NET_IFACE_STATE_INIT`.
3. Calls `ops->init` if it is non-NULL. Wi-Fi skips this.
4. Inserts the handle at the head of `s_iface_list` under the list lock. Lookups by type stop at the first match.
5. `try_register_global_handlers()` installs shared listeners on the default event loop, if that loop already exists:
   - `NET_IFACE_EVENT` → `on_global_iface_event`, which records `s_event_loop_task` before an application callback runs
   - `IP_EVENT` → `on_global_ip_event`
   - `OPENTHREAD_EVENT` → `on_global_ot_event` when Thread is enabled

If the event loop does not exist yet, registration is skipped and `net_iface_start()` tries again. No Wi-Fi API has been called. The handle is only memory, a mutex, and an event group.

---

## Flow 2: `net_iface_start()` returns before the radio is up

`net_iface_start()` (`net_iface.c`, lines 1000–1064):

```c
esp_err_t net_iface_start(net_iface_handle_t h)
{
    // ...
    try_register_global_handlers();

    xSemaphoreTake(h->lock, portMAX_DELAY);
    if ((h->state != NET_IFACE_STATE_INIT && h->state != NET_IFACE_STATE_FAILED) ||
            h->startup_task != NULL) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    // ...
    xEventGroupClearBits(h->state_events, NET_IFACE_BITS_LIVENESS | NET_IFACE_BIT_STARTUP_DONE);
    h->state = NET_IFACE_STATE_STARTING;
    // ...
    BaseType_t created = xTaskCreate(iface_startup_task, task_name, stack_size,
                                     h, NET_IFACE_STARTUP_TASK_PRIO, &h->startup_task);
    // ...
    xSemaphoreGive(h->lock);
    return ESP_OK;
}
```

1. Calls `try_register_global_handlers()` again, in case the event loop was created after the handle.
2. Under `h->lock`, allows the call only from `INIT` or `FAILED`, and only when `h->startup_task` is NULL.
3. Clears `NET_IFACE_BITS_LIVENESS` and `NET_IFACE_BIT_STARTUP_DONE`, so a waiter cannot observe the previous attempt. `STARTUP_DONE` is not part of the liveness set, because a disconnect must not forget that the startup task is still running.
4. Sets `h->state = NET_IFACE_STATE_STARTING`.
5. Creates `iface_startup_task` with `xTaskCreate` **while still holding `h->lock`**. The startup task priority is `NET_IFACE_STARTUP_TASK_PRIO` (5), so it can run before `xTaskCreate` returns. The lock keeps that task from clearing `h->startup_task` before the handle is stored.
6. Releases the lock and returns `ESP_OK`. The caller is not waiting for association or DHCP.

Stack size is `CONFIG_NET_CONNECT_STARTUP_TASK_STACK_SIZE` unless `cfg.startup_task_stack_size > 0`. If `xTaskCreate` fails, the handle goes to `FAILED` with `ESP_ERR_NO_MEM`, `STARTUP_DONE` is set (there is no task to wait for), and `NET_IFACE_EVENT_FAILED` is posted.

`iface_startup_task()` (`net_iface.c`, lines 943–987) calls `h->ops->start(h)` **without** holding `h->lock`:

```c
static void iface_startup_task(void *arg)
{
    net_iface_handle_t h = (net_iface_handle_t)arg;

    esp_err_t err = h->ops->start(h);
    if (err != ESP_OK) {
        net_iface_notify_failed(h, err);
        goto done;
    }

    /* stop() may have arrived while the driver was starting. */
    xSemaphoreTake(h->lock, portMAX_DELAY);
    bool canceled = h->cancel_requested || (h->state == NET_IFACE_STATE_STOPPING);
    bool auto_connect = h->cfg.auto_connect_on_start;
    net_iface_state_t state = h->state;
    xSemaphoreGive(h->lock);

    if (!canceled && auto_connect && state == NET_IFACE_STATE_STARTED) {
        (void)net_iface_connect(h);
    }

done: {
        uint32_t hwm_bytes = (uint32_t)uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
        xSemaphoreTake(h->lock, portMAX_DELAY);
        h->startup_stack_hwm = hwm_bytes;
        h->startup_task = NULL;
        xEventGroupSetBits(h->state_events, NET_IFACE_BIT_STARTUP_DONE);
        xSemaphoreGive(h->lock);
    }

    vTaskDelete(NULL);
}
```

For Wi-Fi, `h->ops->start(h)` is `wifi_ops_start()` (`wifi_connect.c`, lines 182–256):

1. `esp_wifi_init()`. `ESP_ERR_INVALID_STATE` means the radio was already initialized by someone else, so `s_wifi_we_inited` stays false and this driver will not call `esp_wifi_deinit()` later.
2. `esp_netif_create_wifi(WIFI_IF_STA, &base)` with `if_desc` from the handle and `route_prio` (default 128).
3. `esp_wifi_set_default_wifi_sta_handlers()`, `esp_wifi_set_storage(WIFI_STORAGE_RAM)`, `esp_wifi_set_mode(WIFI_MODE_STA)`, `esp_wifi_start()`.
4. Registers `WIFI_EVENT_STA_CONNECTED` → `on_wifi_sta_connected` and `WIFI_EVENT_STA_DISCONNECTED` → `on_wifi_sta_disconnected`, passing the handle as the handler argument.
5. Calls `net_iface_notify_started(self, netif)`.

`net_iface_notify_started()` (`net_iface.c`, lines 440–466) stores `h->netif`, moves the state to `STARTED`, clears `NET_IFACE_BIT_STOPPED`, and posts `NET_IFACE_EVENT_STARTED` with a post timeout of 0. If the state is already `STOPPING`, it still stores `h->netif` and does **not** move to `STARTED`. That is how a stop that races with startup still knows which netif to free.

Back in the startup task, if stop has not been requested, `auto_connect_on_start` is true, and the state is `STARTED`, it calls `net_iface_connect()`. That moves the state to `CONNECTING` and calls `wifi_ops_connect()`:

- empty SSID returns `ESP_ERR_INVALID_STATE` (see Flow 5)
- otherwise `esp_wifi_set_config(WIFI_IF_STA, ...)` and `esp_wifi_connect()`, which return without waiting for an IP

The task then stores `uxTaskGetStackHighWaterMark()` in `h->startup_stack_hwm`, clears `h->startup_task`, sets `NET_IFACE_BIT_STARTUP_DONE`, and deletes itself with `vTaskDelete(NULL)`. The Idle task frees that stack.

---

## Flow 3: Link up, then IP, then `READY`

These steps run on the default event loop, not on the task that called `net_iface_start()`.

**Layer 2.** `WIFI_EVENT_STA_CONNECTED` runs `on_wifi_sta_connected()` (`wifi_connect.c`, lines 123–130), which calls `net_iface_notify_connected()`. ESP-IDF's default STA handlers, installed by `esp_wifi_set_default_wifi_sta_handlers()`, start the DHCP client on the same event.

`net_iface_notify_connected()` (`net_iface.c`, lines 474–505):

```c
void net_iface_notify_connected(net_iface_handle_t h)
{
    // ...
    h->state = NET_IFACE_STATE_CONNECTED;
    h->retry_num = 0;
    netif = h->netif;
    enable_ipv6 = h->cfg.ip.enable_ipv6;
    type = h->cfg.type;
    xSemaphoreGive(h->lock);

#if CONFIG_NET_CONNECT_IPV6
    if (netif != NULL && enable_ipv6 && type != NET_IFACE_TYPE_THREAD) {
        esp_err_t err = esp_netif_create_ip6_linklocal(netif);
        // ...
    }
#endif
    post_iface_event(h, NET_IFACE_EVENT_CONNECTED, netif, NET_IFACE_STATE_CONNECTED, ESP_OK);
}
```

- moves `CONNECTING` → `CONNECTED` and zeroes `retry_num`
- after releasing `h->lock`, calls `esp_netif_create_ip6_linklocal()` when IPv6 is enabled and this is not Thread
- posts `NET_IFACE_EVENT_CONNECTED`

`CONNECTED` means the AP association is up. It does not mean an address is present.

**Layer 3.** `on_global_ip_event()` (`net_iface.c`, lines 661–681) handles every interface:

```c
static void on_global_ip_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    s_event_loop_task = xTaskGetCurrentTaskHandle();
    // ...
    if (event_id == IP_EVENT_STA_GOT_IP ||
            event_id == IP_EVENT_ETH_GOT_IP ||
            event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        set_readiness_bit(find_iface_by_netif(event->esp_netif), NET_IFACE_BIT_IPV4);
        // ...
#if CONFIG_NET_CONNECT_IPV6
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        if (esp_netif_ip6_get_addr_type(&event->ip6_info.ip) == NET_CONNECT_PREFERRED_IPV6_TYPE) {
            set_readiness_bit(find_iface_by_netif(event->esp_netif), NET_IFACE_BIT_IPV6);
        }
#endif
    }
}
```

- `IP_EVENT_STA_GOT_IP` (also Ethernet and PPP got-IP) → `find_iface_by_netif()` → `set_readiness_bit(..., NET_IFACE_BIT_IPV4)`
- `IP_EVENT_GOT_IP6` counts only when `esp_netif_ip6_get_addr_type()` equals `NET_CONNECT_PREFERRED_IPV6_TYPE`, then sets `NET_IFACE_BIT_IPV6`
- lost-IP events are Flow 6

`find_iface_by_netif()` takes the list lock, then each interface lock, and matches `cur->netif`.

`set_readiness_bit()` sets the bit and calls `is_ready_satisfied_locked()` (`net_iface.c`, lines 363–373) while `h->lock` is held:

- Thread is ready on `NET_IFACE_BIT_OT_ATTACHED` only
- Wi-Fi, Ethernet, and PPP need `NET_IFACE_BIT_IPV4` when `CONFIG_NET_CONNECT_IPV4` is on
- they also need `NET_IFACE_BIT_IPV6` when `CONFIG_NET_CONNECT_IPV6` is on **and** `h->cfg.ip.enable_ipv6` is true

When that check passes, the lock is released and `net_iface_notify_ready()` (`net_iface.c`, lines 523–537) runs:

- state becomes `READY`, `last_err` becomes `ESP_OK`
- `FAILED` and `STOPPED` are cleared, `NET_IFACE_BIT_READY` is set (this wakes `net_iface_wait()`)
- `pulse_global_wait_events()` sets `NET_IFACE_GLOBAL_WAKE_BIT` (this wakes `wait_all` / `wait_any`)
- `NET_IFACE_EVENT_READY` is posted

`READY` means the required local address is present. It does not mean the device can reach the internet.

---

## Flow 4: `net_iface_wait`, `wait_all`, and `wait_any`

**Event-loop guard.** `on_global_iface_event()` and `on_global_ip_event()` store `s_event_loop_task`. `called_from_event_loop()` (`net_iface.c`, lines 90–94) compares that with `xTaskGetCurrentTaskHandle()`. `net_iface_wait()`, `wait_all()`, `wait_any()`, and `net_iface_stop()` return `ESP_ERR_INVALID_STATE` immediately when called from that task. A wait there would block the task that has to deliver `IP_EVENT_STA_GOT_IP`.

**One handle.** `net_iface_wait()` (`net_iface.c`, lines 1214–1248) sleeps on:

```c
xEventGroupWaitBits(h->state_events,
                    NET_IFACE_BIT_READY | NET_IFACE_BIT_FAILED | NET_IFACE_BIT_STOPPED,
                    pdFALSE, pdFALSE, ticks);
```

Both `pdFALSE` arguments matter. The bits are not cleared by the wait, and any one bit wakes the caller. Consequences:

- If the address arrived before `net_iface_wait()`, `READY` is already set and the call returns `ESP_OK` immediately.
- Two tasks can wait on the same handle and both wake up.
- `READY` is checked before `FAILED`, and `FAILED` before `STOPPED`.
- `ESP_ERR_TIMEOUT` leaves the station associating or retrying. Nothing is stopped.

`UINT32_MAX` waits with `portMAX_DELAY`. A handle still in `INIT` or `UNINIT` returns `ESP_ERR_INVALID_STATE` without sleeping.

**Several handles.** FreeRTOS cannot wait on more than one event group in a single call. `s_global_wait_events` is only a pulse: "look again". Each handle still owns its sticky bits.

`net_iface_wait_all()` (`net_iface.c`, lines 1315–1368) and `net_iface_wait_any()` (lines 1388–1465) share one timeout budget from the tick count at entry (`wait_ticks_left()`). Each loop:

1. Clears `NET_IFACE_GLOBAL_WAKE_BIT` **before** scanning.
2. Reads each handle's bits.
   - `wait_all` returns `ESP_OK` when every handle has `READY`. The first `FAILED` or `STOPPED` returns immediately.
   - `wait_any` returns `ESP_OK` and optionally writes `*out_ready` as soon as one handle has `READY`. One failure does not end the wait while another handle is still trying. It returns an error only when every handle has failed or stopped.
3. Sleeps on `NET_IFACE_GLOBAL_WAKE_BIT` for the time still left.

Clearing the pulse before the scan is what avoids a lost wakeup. If `notify_ready()` runs during the scan, it sets the bit again, and the following wait returns at once.

---

## Flow 5: Radio up, credentials later

This is the path when the SSID is empty at create time (unprovisioned station, or stdin credentials that have not been read yet).

1. `fill_iface_config()` sets `auto_connect_on_start` from `sta->ssid[0] != '\0'` (`wifi_connect.c`, lines 115–116). `net_wifi_sta_create()` forces it false again when the Kconfig SSID is empty (lines 393–396).
2. `net_iface_start()` still runs `wifi_ops_start()`. The radio and netif come up. The startup task does not call `net_iface_connect()` because `auto_connect_on_start` is false. The handle stays in `STARTED`.
3. If something does call `net_iface_connect()` with an empty SSID, `wifi_ops_connect()` (`wifi_connect.c`, lines 270–273) returns `ESP_ERR_INVALID_STATE`. `net_iface_connect()` (`net_iface.c`, lines 1094–1105) puts the state back to `STARTED` instead of `FAILED`.
4. Later, `net_wifi_sta_set_credentials()` (`wifi_connect.c`, lines 401–419) is allowed in `INIT`, `STARTED`, or `FAILED`. It copies the SSID and password into `h->cfg.wifi_sta` under `h->lock`. It does not call `esp_wifi_set_config()`.
5. A following `net_iface_connect()` moves `STARTED` → `CONNECTING`, clears `READY` and `FAILED`, and `wifi_ops_connect()` calls `esp_wifi_set_config()` and `esp_wifi_connect()`. Flow 3 then runs as usual.

---

## Flow 6: Address loss, disconnect, retry, and `FAILED`

**Address lost, link still up.** `IP_EVENT_STA_LOST_IP` in `on_global_ip_event()` calls `net_iface_notify_lost_ip()` (`net_iface.c`, lines 550–569). From `READY` or `CONNECTED` the state goes back to `CONNECTING`, `READY` and `IPV4` bits are cleared, and `NET_IFACE_EVENT_LOST_IP` is posted. The driver is still running and may get another address. This does not pulse the global wait group, because the interface has not reached a terminal result.

**AP dropped.** `WIFI_EVENT_STA_DISCONNECTED` runs `on_wifi_sta_disconnected()` (`wifi_connect.c`, lines 133–172):

- returns immediately when `s_wifi_ignore_disconnect` is set (an intentional disconnect or stop) or when the handle is `STOPPING`, `INIT`, or `STARTED`
- returns on `WIFI_REASON_ROAMING`
- increments `h->retry_num` under the lock
- if `retry_num <= max_retry`, calls `esp_wifi_connect()` again
- if `retry_num > max_retry`, calls `net_iface_notify_failed(self, ESP_FAIL)`

`net_iface_notify_failed()` (`net_iface.c`, lines 600–628) stores a real error (`ESP_OK` is rewritten to `ESP_FAIL`), sets `FAILED`, clears `READY`, sets `NET_IFACE_BIT_FAILED`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_FAILED`. A blocked `net_iface_wait()` then returns `h->last_err`.

A later `net_iface_connect()` or `net_iface_start()` is allowed from `FAILED`. Both clear the old `READY` and `FAILED` bits before the new attempt.

---

## Flow 7: Disconnect, stop during startup, and destroy

**Drop the association only.** `net_iface_disconnect()` (`net_iface.c`, lines 1113–1143) is valid in `CONNECTING`, `CONNECTED`, `READY`, or `FAILED`. It calls `wifi_ops_disconnect()` (`wifi_connect.c`, lines 288–298), which sets `s_wifi_ignore_disconnect` so the disconnect event does not start another join, then calls `esp_wifi_disconnect()`. `net_iface_notify_disconnected()` sets the state to `STARTED`, clears `NET_IFACE_BITS_LIVENESS`, and posts `NET_IFACE_EVENT_DISCONNECTED`. The radio and the netif stay up.

**Stop the driver.** `net_iface_stop()` (`net_iface.c`, lines 1152–1203):

1. Returns `ESP_ERR_INVALID_STATE` on the event-loop task.
2. Returns `ESP_OK` if the state is already `INIT` or `STOPPING`.
3. If `h->startup_task != NULL`, sets `cancel_requested` and `STOPPING`, then waits up to `NET_IFACE_STARTUP_STOP_WAIT_MS` (10000 ms) for `NET_IFACE_BIT_STARTUP_DONE`.
4. The startup task, if it is inside `wifi_ops_start()`, finishes that call. `net_iface_notify_started()` sees `STOPPING`, saves `h->netif`, and does not enter `STARTED`. The task then sees `cancel_requested`, skips `net_iface_connect()`, sets `STARTUP_DONE`, and exits.
5. `wifi_ops_disconnect()` then `wifi_ops_stop()` (`wifi_connect.c`, lines 301–326): unregister the two `WIFI_EVENT` handlers, `esp_wifi_stop()`, `esp_wifi_deinit()` only when `s_wifi_we_inited` is true, `esp_wifi_clear_default_wifi_driver_and_handlers()`, `esp_netif_destroy()`.
6. `net_iface_notify_stopped()` (`net_iface.c`, lines 635–652) clears `h->netif`, sets `INIT`, clears liveness bits, sets `STOPPED`, pulses group waits, and posts `NET_IFACE_EVENT_STOPPED`. The handle is marked stopped even if `ops->stop` returned an error, so it cannot stick in `STOPPING`.

**Free the handle.** `net_iface_deinit()` (`net_iface.c`, lines 895–930) takes `s_registry_mutex` and then `h->lock`. The state must be `INIT`. It unlinks the handle, releases both locks, and `maybe_unregister_global_handlers()` drops the shared IP / `NET_IFACE_EVENT` listeners when the list is empty. `wifi_ops_deinit()` clears `s_wifi_sta_handle` if it pointed at this handle. `free_iface_obj()` deletes the event group and mutex and frees the object. `destroy_registry_mutex_if_idle()` deletes `s_global_wait_events` and `s_registry_mutex` when nothing remains, so a unit-test leak check can return to the baseline heap.

---

## What this walkthrough does not cover

Ethernet, Thread, and PPP use the same `net_iface_start` / notify / wait / stop path. Their driver-specific walkthroughs are in:

- [CORE_LIFECYCLE_ETH.md](./CORE_LIFECYCLE_ETH.md) — Flow 9 (Ethernet)
- [CORE_LIFECYCLE_THREAD.md](./CORE_LIFECYCLE_THREAD.md) — Flow 10 (OpenThread)
- [CORE_LIFECYCLE_PPP.md](./CORE_LIFECYCLE_PPP.md) — Flow 11 (PPP over USB CDC and UART)

`net_connect()`, route priority changes after `READY`, stdin credentials, and the console command are Flows 12–14 in [IMPLEMENTATION_FLOWS.md](./IMPLEMENTATION_FLOWS.md).
