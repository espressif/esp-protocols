# Core lifecycle, walked through with Ethernet

This is the end-to-end path for **Flows 1–7 and Flow 9** in [IMPLEMENTATION_FLOWS.md](./IMPLEMENTATION_FLOWS.md), using Ethernet (`eth_connect.c`) as the concrete driver. The same core (`net_iface.c`) is shared by Wi-Fi STA, OpenThread, and PPP. Only the `net_iface_ops_t` callbacks and event handlers differ.

Sources:

- `include/net_iface_priv.h` — handle, ops table, sticky event-group bits
- `net_iface.c` — registry, state machine, waits, notifications
- `eth_connect.c` — `g_net_iface_eth_ops` and the ESP-IDF Ethernet / `ethernet_init` calls

Line numbers below match those files as of this write-up.

---

## Call sequence

The application calls `net_eth_create()` to create the Ethernet handle. Start, connect, wait, disconnect, stop, and destroy go to `net_iface`, which calls the Ethernet ops. Ethernet drives `espressif/ethernet_init` and `esp_eth`, and reports back with `net_iface_notify_*()`. Link and IP events arrive on the default event loop.

Happy path: `INIT` → `STARTING` → `STARTED` → `CONNECTING` → `CONNECTED` → `READY`. `net_iface_start()` returns while the startup task is still initializing the MAC and PHY.

### Flows 1–4: create through ready

```mermaid
sequenceDiagram
    participant App as User Application
    participant Eth as Ethernet (eth_connect.c)
    participant Core as net_iface Core
    participant Task as Startup Task
    participant IDF as ESP-IDF esp_eth / netif
    participant Loop as Default Event Loop

    rect 1. Create handle (UNINIT -> INIT)
    App->>Eth: net_eth_create()
    Eth->>Eth: Fill defaults (route_prio = 64, auto_connect_on_start = true), apply params
    Eth->>Core: net_iface_init_with_ops(g_net_iface_eth_ops)
    Core->>Core: Allocate handle, state = INIT (ops->init is NULL)
    opt Event loop exists and global handlers not registered yet
        Core->>Loop: Register (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return ESP_OK
    end
    Core-->>Eth: Return handle
    Eth-->>App: Return handle (ESP_OK)
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
    Task->>Eth: ops->start() (eth_ops_start)
    Eth->>IDF: ethernet_init_all(&s_eth_handles, &s_eth_count)
    IDF-->>Eth: Return MAC/PHY handles
    Eth->>IDF: esp_netif_new(), esp_eth_new_netif_glue(s_eth_handles[0]), esp_netif_attach()
    IDF->>Loop: Glue registers default ETH netif handlers (ETH_EVENT START/STOP/CONNECTED/DISCONNECTED, IP_EVENT_ETH_GOT_IP)
    Loop-->>IDF: Return ESP_OK
    IDF-->>Eth: Return ESP_OK
    Eth->>Loop: Register (ETH_EVENT, ESP_EVENT_ANY_ID) -> on_eth_event
    Loop-->>Eth: Return ESP_OK
    Eth->>Core: net_iface_notify_started(s_eth_netif)
    Core->>Core: Save netif, state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_STARTED
    Loop-->>Core: Queued
    Core-->>Eth: Return from net_iface_notify_started()
    Eth-->>Task: Return ESP_OK from eth_ops_start()
    end

    rect 4. Auto-connect (STARTED -> CONNECTING)
    Note over Task,Core: auto_connect_on_start is true and stop was not requested
    Task->>Core: net_iface_connect()
    Core->>Core: Clear READY/FAILED bits, state = CONNECTING
    Core->>Eth: ops->connect() (eth_ops_connect)
    Eth->>IDF: esp_eth_start(s_eth_handles[0]) (starts PHY autonegotiation)
    IDF-->>Eth: Return ESP_OK
    Eth-->>Core: Return ESP_OK from eth_ops_connect()
    Core-->>Task: Return ESP_OK from net_iface_connect()
    Task->>Core: Save stack HWM, set STARTUP_DONE
    Core-->>Task: Done
    Task->>Task: vTaskDelete(NULL)
    end

    rect 5. Cable link up (CONNECTING -> CONNECTED)
    IDF->>Loop: Post ETHERNET_EVENT_CONNECTED
    Loop->>IDF: Default ETH glue handler starts DHCP client on netif
    IDF-->>Loop: Return
    Loop->>Eth: on_eth_event(ETHERNET_EVENT_CONNECTED)
    Eth->>Core: net_iface_notify_connected()
    Core->>Core: state = CONNECTED, retry_num = 0
    opt IPv6 enabled
        Core->>IDF: esp_netif_create_ip6_linklocal()
        IDF-->>Core: Return
    end
    Core->>Loop: Post NET_IFACE_EVENT_CONNECTED
    Loop-->>Core: Queued
    Core-->>Eth: Return from net_iface_notify_connected()
    Eth-->>Loop: Return from on_eth_event()
    end

    rect 6. Address acquired (CONNECTED -> READY)
    IDF->>Loop: Post IP_EVENT_ETH_GOT_IP (and IP_EVENT_GOT_IP6 if required)
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

`net_iface_disconnect()` stops the PHY via `esp_eth_stop()` while keeping the MAC, glue, and `esp_netif` allocated in `STARTED`. `net_iface_stop()` unregisters `on_eth_event`, deletes the netif glue, destroys the netif, deinitializes the Ethernet hardware, and returns the handle to `INIT`.

```mermaid
sequenceDiagram
    participant App as User Application
    participant Eth as Ethernet (eth_connect.c)
    participant Core as net_iface Core
    participant IDF as ESP-IDF esp_eth / netif
    participant Loop as Default Event Loop

    rect 1. Disconnect (READY/CONNECTED -> STARTED)
    App->>Core: net_iface_disconnect()
    Core->>Eth: ops->disconnect() (eth_ops_disconnect)
    Eth->>IDF: esp_eth_stop(s_eth_handles[0])
    IDF-->>Eth: Return ESP_OK
    Eth-->>Core: Return ESP_OK from eth_ops_disconnect()
    Core->>Core: net_iface_notify_disconnected() -> state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_DISCONNECTED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK (netif, glue, and MAC stay allocated)
    end

    rect 2. Stop driver (STARTED -> STOPPING -> INIT)
    App->>Core: net_iface_stop()
    Core->>Core: state = STOPPING (wait for STARTUP_DONE if task is running)
    Core->>Eth: ops->disconnect() (eth_ops_disconnect)
    Eth->>IDF: esp_eth_stop(s_eth_handles[0])
    IDF-->>Eth: Return ESP_OK (or ESP_ERR_INVALID_STATE -> ESP_OK)
    Eth-->>Core: Return ESP_OK from eth_ops_disconnect()
    Core->>Eth: ops->stop() (eth_ops_stop -> eth_release_resources)
    Eth->>Loop: Unregister (ETH_EVENT, ESP_EVENT_ANY_ID) -> on_eth_event
    Loop-->>Eth: Return ESP_OK
    Eth->>IDF: esp_eth_stop(s_eth_handles[0])
    IDF-->>Eth: Return
    Eth->>IDF: esp_eth_del_netif_glue(s_eth_glue)
    IDF->>Loop: Unregister default ETH glue handlers
    Loop-->>IDF: Return ESP_OK
    IDF-->>Eth: Return ESP_OK
    Eth->>IDF: esp_netif_destroy(s_eth_netif) and ethernet_deinit_all(s_eth_handles)
    IDF-->>Eth: Return
    Eth-->>Core: Return ESP_OK from eth_ops_stop()
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
    Note over Core,Eth: ops->deinit is NULL for Ethernet
    Core->>Core: Free event group, mutex, and handle (state = UNINIT)
    Core-->>App: Return ESP_OK
    end
```

### Flows 5–6 and stop during startup

Manual connect (`auto_connect_on_start = false`), cable unplug / IP loss, and a stop that arrives while `eth_ops_start()` is still running.

```mermaid
sequenceDiagram
    participant App as User Application
    participant Eth as Ethernet (eth_connect.c)
    participant Core as net_iface Core
    participant Task as Startup Task
    participant IDF as ESP-IDF esp_eth / netif
    participant Loop as Default Event Loop

    rect Flow 5 - Start without auto-connect, connect later (INIT -> STARTED -> CONNECTING)
    App->>Eth: 1. net_eth_create() with auto_connect_on_start = false
    Eth->>Core: net_iface_init_with_ops(g_net_iface_eth_ops)
    Core-->>Eth: Return handle
    Eth-->>App: Return handle (ESP_OK)
    App->>Core: 2. net_iface_start()
    Core->>Task: xTaskCreate(iface_startup_task)
    Core-->>App: Return ESP_OK
    Task->>Eth: ops->start() (eth_ops_start)
    Eth->>Core: net_iface_notify_started(s_eth_netif) -> state = STARTED
    Core-->>Eth: Return
    Eth-->>Task: Return ESP_OK from eth_ops_start()
    Note over Task,Core: auto_connect_on_start is false, esp_eth_start() is not called yet
    Task->>Core: Set STARTUP_DONE
    Core-->>Task: Done
    Task->>Task: vTaskDelete(NULL)
    App->>Core: 3. net_iface_connect()
    Core->>Eth: ops->connect() (eth_ops_connect)
    Eth->>IDF: esp_eth_start(s_eth_handles[0])
    IDF-->>Eth: Return ESP_OK
    Eth-->>Core: Return ESP_OK from eth_ops_connect()
    Core-->>App: Return ESP_OK from net_iface_connect()
    end

    rect Flow 6a - Cable unplugged (READY/CONNECTED -> CONNECTING)
    IDF->>Loop: 4. Post ETHERNET_EVENT_DISCONNECTED
    Loop->>Eth: on_eth_event(ETHERNET_EVENT_DISCONNECTED)
    Eth->>Core: net_iface_notify_lost_ip()
    Core->>Core: Clear READY and IPV4 bits, state = CONNECTING
    Core->>Loop: Post NET_IFACE_EVENT_LOST_IP
    Loop-->>Core: Queued
    Core-->>Eth: Return from net_iface_notify_lost_ip()
    Eth-->>Loop: Return from on_eth_event()
    end

    rect Flow 6b - DHCP lease lost (READY -> CONNECTING)
    IDF->>Loop: 5. Post IP_EVENT_ETH_LOST_IP
    Loop->>Core: on_global_ip_event()
    Core->>Core: net_iface_notify_lost_ip() -> clear READY and IPV4 bits, state = CONNECTING
    Core->>Loop: Post NET_IFACE_EVENT_LOST_IP
    Loop-->>Core: Queued
    Core-->>Loop: Return from on_global_ip_event()
    end

    rect Flow 7 - Stop during startup (STARTING -> STOPPING -> INIT)
    Note over App,Task: 6a. App calls stop() while Startup Task is still inside eth_ops_start()
    Task->>Eth: ops->start() (eth_ops_start) running
    App->>Core: net_iface_stop()
    Core->>Core: cancel_requested = true, state = STOPPING
    Note over App,Core: App task sleeps on STARTUP_DONE bit (up to 10 s)

    Note over Eth,Task: 6b. Startup Task finishes eth_ops_start(), sees cancel_requested, and exits
    Eth->>Core: net_iface_notify_started(s_eth_netif)
    Core->>Core: State is STOPPING -> save h->netif only (do not enter STARTED)
    Core-->>Eth: Return from net_iface_notify_started()
    Eth-->>Task: Return ESP_OK from eth_ops_start()
    Task->>Core: Check cancel_requested == true -> skip net_iface_connect()
    Task->>Core: Set STARTUP_DONE bit (wakes App task in net_iface_stop)
    Task->>Task: vTaskDelete(NULL)

    Note over App,Loop: 6c. App task wakes up inside net_iface_stop() and tears down the driver
    Core->>Eth: ops->disconnect() (eth_ops_disconnect)
    Eth->>IDF: esp_eth_stop(s_eth_handles[0])
    IDF-->>Eth: Return ESP_OK
    Eth-->>Core: Return ESP_OK
    Core->>Eth: ops->stop() (eth_ops_stop -> eth_release_resources)
    Eth->>Loop: Unregister (ETH_EVENT, ESP_EVENT_ANY_ID) -> on_eth_event
    Loop-->>Eth: Return ESP_OK
    Eth->>IDF: esp_eth_del_netif_glue(), esp_netif_destroy(), ethernet_deinit_all()
    IDF-->>Eth: Return ESP_OK
    Eth-->>Core: Return ESP_OK
    Core->>Core: net_iface_notify_stopped() -> state = INIT, set STOPPED bit
    Core->>Loop: Post NET_IFACE_EVENT_STOPPED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK from net_iface_stop()
    end
```

---

## The objects the driver keeps

Ethernet fills the ops table in `eth_connect.c` (lines 166–175):

```c
const net_iface_ops_t g_net_iface_eth_ops = {
    .name = "eth",
    .init = NULL,
    .deinit = NULL,
    .start = eth_ops_start,
    .stop = eth_ops_stop,
    .connect = eth_ops_connect,
    .disconnect = eth_ops_disconnect,
};
```

Both `init` and `deinit` are `NULL`:

- Creating an Ethernet handle does not touch the MAC, PHY, or SPI bus.
- Stopping the interface releases all hardware and netif resources (`eth_release_resources()`), so `net_iface_deinit()` only frees the `struct net_iface` handle.

Module-level state in `eth_connect.c` (lines 27–30):

```c
static esp_eth_handle_t *s_eth_handles = NULL;
static uint8_t s_eth_count = 0;
static esp_eth_netif_glue_handle_t s_eth_glue = NULL;
static esp_netif_t *s_eth_netif = NULL;
```

Only `s_eth_handles[0]` (the first port returned by `ethernet_init_all()`) is attached to `s_eth_netif`.

---

## Flow 1: Create the handle

Public call: `net_eth_create()` in `eth_connect.c` (lines 177–213):

```c
esp_err_t net_eth_create(const net_eth_params_t *params, net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    net_iface_config_t cfg = { 0 };
    cfg.type = NET_IFACE_TYPE_ETH;
    cfg.desc = NET_CONNECT_NETIF_DESC_ETH;
    cfg.route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO;
    // ...
    return net_iface_init_with_ops(&cfg, &g_net_iface_eth_ops, out_handle);
}
```

1. Builds `net_iface_config_t`:
   - `type = NET_IFACE_TYPE_ETH`
   - `desc = NET_CONNECT_NETIF_DESC_ETH` (`"net_connect_netif_eth"`)
   - `route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO` (64)
   - `required` from `CONFIG_NET_CONNECT_ETHERNET_REQUIRED`
   - `auto_connect_on_start = true`
   - `ip.use_dhcp = true`, and `ip.enable_ipv6` from `CONFIG_NET_CONNECT_IPV6`
2. A non-NULL `net_eth_params_t` overrides `route_prio`, `required`, `auto_connect_on_start`, and `ip`.
3. Calls `net_iface_init_with_ops(&cfg, &g_net_iface_eth_ops, out_handle)` to allocate the handle in `NET_IFACE_STATE_INIT`, prepend it to `s_iface_list`, and call `try_register_global_handlers()`.

---

## Flow 2: `net_iface_start()` and `eth_ops_start()`

`net_iface_start()` moves the handle from `INIT` to `STARTING` and spawns `iface_startup_task` (`net_iface.c`, lines 1000–1064). That keeps any slow MAC/PHY initialization off the caller's task.

Inside `iface_startup_task`, `h->ops->start(h)` calls `eth_ops_start()` (`eth_connect.c`, lines 80–132):

1. Rejects a second start if `s_eth_netif != NULL` (`ESP_ERR_INVALID_STATE`).
2. Calls `ethernet_init_all(&s_eth_handles, &s_eth_count)` from `espressif/ethernet_init`. If this fails or returns zero ports, `eth_ops_start()` returns an error and `iface_startup_task` marks the handle `FAILED` via `net_iface_notify_failed()`.
3. Creates `s_eth_netif` using `ESP_NETIF_INHERENT_DEFAULT_ETH()`, setting `if_desc` (`"net_connect_netif_eth"`) and `route_prio` (default 64).
4. Creates the netif glue with `esp_eth_new_netif_glue(s_eth_handles[0])` and attaches it with `esp_netif_attach(s_eth_netif, s_eth_glue)`. Inside ESP-IDF, `esp_eth_new_netif_glue()` registers the default netif glue handlers on `ETH_EVENT` (`START`, `STOP`, `CONNECTED`, `DISCONNECTED`) and `IP_EVENT_ETH_GOT_IP`.
5. Registers `(ETH_EVENT, ESP_EVENT_ANY_ID)` → `on_eth_event` with the interface handle `self`.
6. Calls `net_iface_notify_started(self, s_eth_netif)` (`STARTING` → `STARTED`, posts `NET_IFACE_EVENT_STARTED`).

Notice that `eth_ops_start()` does **not** call `esp_eth_start()`. The PHY stays stopped until `eth_ops_connect()`.

If `auto_connect_on_start` is true and `net_iface_stop()` was not requested while `eth_ops_start()` was running, `iface_startup_task` immediately calls `net_iface_connect(h)`, which transitions `STARTED` → `CONNECTING` and calls `eth_ops_connect()` (`eth_connect.c`, lines 135–142):

```c
static esp_err_t eth_ops_connect(net_iface_handle_t self)
{
    (void)self;
    if (s_eth_handles == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_eth_start(s_eth_handles[0]);
}
```

`esp_eth_start()` starts the Ethernet MAC/PHY state machine. `iface_startup_task` then records `startup_stack_hwm`, sets `NET_IFACE_BIT_STARTUP_DONE`, and deletes itself.

---

## Flow 3: Link up, then IP, then `READY`

1. **Cable link up.** When the PHY completes autonegotiation, `esp_eth` posts `ETHERNET_EVENT_CONNECTED` on the default event loop:
   - ESP-IDF's netif glue handler starts the DHCP client on `s_eth_netif`.
   - `on_eth_event()` (`eth_connect.c`, lines 33–51) calls `net_iface_notify_connected(self)`.
   - `net_iface_notify_connected()` moves `CONNECTING` → `CONNECTED`, resets `retry_num = 0`, calls `esp_netif_create_ip6_linklocal()` when IPv6 is enabled, and posts `NET_IFACE_EVENT_CONNECTED`.
2. **IP acquired.** When DHCP succeeds, `IP_EVENT_ETH_GOT_IP` runs `on_global_ip_event()` (`net_iface.c`, lines 661–694):
   - Matches `event->esp_netif` to the Ethernet handle via `find_iface_by_netif()`.
   - Sets `NET_IFACE_BIT_IPV4` (and `IP_EVENT_GOT_IP6` sets `NET_IFACE_BIT_IPV6` when a preferred IPv6 address arrives).
   - Once `is_ready_satisfied_locked()` passes, `net_iface_notify_ready()` transitions `CONNECTED` → `READY`, sets `NET_IFACE_BIT_READY`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_READY`.

---

## Flow 6: Cable unplug and IP loss

Unlike Wi-Fi STA, Ethernet does not have a retry counter or a `FAILED` transition on link drop—the PHY hardware continuously monitors the physical link while started:

- **Cable unplugged (`ETHERNET_EVENT_DISCONNECTED`):** `on_eth_event()` (`eth_connect.c`, lines 44–47) calls `net_iface_notify_lost_ip(self)`. That moves `READY` or `CONNECTED` back to `CONNECTING`, clears `NET_IFACE_BIT_READY` and `NET_IFACE_BIT_IPV4`, and posts `NET_IFACE_EVENT_LOST_IP`. Because the PHY stays started, plugging the cable back in raises `ETHERNET_EVENT_CONNECTED` and recovers automatically to `CONNECTED` → `READY`.
- **DHCP lease lost (`IP_EVENT_ETH_LOST_IP`):** `on_global_ip_event()` also calls `net_iface_notify_lost_ip()`, moving the handle back to `CONNECTING` until a new address arrives.

---

## Flow 7: Disconnect, stop, and destroy

- **Drop the link only (`net_iface_disconnect`):** Calls `eth_ops_disconnect()` (`eth_connect.c`, lines 145–156), which calls `esp_eth_stop(s_eth_handles[0])` (treating `ESP_ERR_INVALID_STATE` as `ESP_OK`). `net_iface_notify_disconnected()` sets the state to `STARTED`, clears liveness bits, and posts `NET_IFACE_EVENT_DISCONNECTED`. The MAC, glue, and `esp_netif` remain allocated, so a subsequent `net_iface_connect()` only needs to call `esp_eth_start(s_eth_handles[0])`.
- **Stop the driver (`net_iface_stop`):** Waits for `NET_IFACE_BIT_STARTUP_DONE` if `iface_startup_task` is still running, calls `eth_ops_disconnect()`, and then calls `eth_ops_stop()` (`eth_connect.c`, lines 159–164), which invokes `eth_release_resources()` (lines 54–77):
  1. `esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event)`
  2. `esp_eth_stop(s_eth_handles[0])`
  3. `esp_eth_del_netif_glue(s_eth_glue)`
  4. `esp_netif_destroy(s_eth_netif)`
  5. `ethernet_deinit_all(s_eth_handles)`
  `net_iface_notify_stopped()` then resets the handle to `INIT`, sets `NET_IFACE_BIT_STOPPED`, pulses group waiters, and posts `NET_IFACE_EVENT_STOPPED`.
- **Free the handle (`net_iface_deinit`):** Unlinks the handle in `INIT`, unregisters the shared `NET_IFACE_EVENT` and `IP_EVENT` handlers if the registry is now empty, and frees the mutex, event group, and handle memory.
