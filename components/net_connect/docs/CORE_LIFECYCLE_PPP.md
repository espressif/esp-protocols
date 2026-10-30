# Core lifecycle, walked through with PPP (USB CDC and UART)

This is the end-to-end path for **Flows 1–7 and Flow 11** in [IMPLEMENTATION_FLOWS.md](./IMPLEMENTATION_FLOWS.md), using PPP (`ppp_connect.c`) as the concrete driver. The same core (`net_iface.c`) is shared by Wi-Fi STA, Ethernet, and OpenThread. PPP supports two compile-time transports selected by Kconfig:

- **USB CDC (`CONFIG_NET_CONNECT_PPP_DEVICE_USB`):** Installs TinyUSB CDC-ACM in `ppp_ops_start()` and starts PPP negotiation directly in `ppp_ops_connect()`.
- **UART (`CONFIG_NET_CONNECT_PPP_DEVICE_UART`):** Creates the PPP `esp_netif` in `ppp_ops_start()` and spawns a dedicated `ppp_task` (`"ppp connect"`) in `ppp_ops_connect()` that installs the UART driver and pumps bytes into `esp_netif_receive()`.

Sources:

- `include/net_iface_priv.h` — handle, ops table, sticky event-group bits
- `net_iface.c` — registry, state machine, waits, notifications
- `ppp_connect.c` — `g_net_iface_ppp_ops`, USB CDC callbacks, `ppp_task`, and `on_ppp_lost_ip`

Line numbers below match those files as of this write-up.

---

## Call sequence

The application calls `net_ppp_create()` to create the PPP handle. Start, connect, wait, disconnect, stop, and destroy go to `net_iface`, which calls the PPP ops. PPP drives `esp_netif` PPP actions plus either TinyUSB CDC-ACM or the ESP-IDF UART driver, and reports back with `net_iface_notify_*()`.

Happy path: `INIT` → `STARTING` → `STARTED` → `CONNECTING` → `CONNECTED` → `READY`. Unlike Wi-Fi and Ethernet (which wait for an asynchronous link event before calling `net_iface_notify_connected()`), `ppp_ops_connect()` calls `net_iface_notify_connected()` immediately once PPP negotiation is started; `READY` then waits for `IP_EVENT_PPP_GOT_IP`.

### Flows 1–4: create through ready

```mermaid
sequenceDiagram
    participant App as User Application
    participant PPP as PPP (ppp_connect.c)
    participant Core as net_iface Core
    participant Task as Startup Task
    participant Worker as UART Task (ppp_task)
    participant IDF as ESP-IDF PPP netif / Transport
    participant Loop as Default Event Loop

    rect 1. Create handle (UNINIT -> INIT)
    App->>PPP: net_ppp_create()
    PPP->>PPP: Fill defaults (route_prio = 32, max_retry, auto_connect_on_start = true), apply params
    PPP->>Core: net_iface_init_with_ops(g_net_iface_ppp_ops)
    Core->>Core: Allocate handle, state = INIT (ops->init is NULL)
    opt Event loop exists and global handlers not registered yet
        Core->>Loop: Register (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return ESP_OK
    end
    Core-->>PPP: Return handle
    PPP-->>App: Return handle (ESP_OK)
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
    Task->>PPP: ops->start() (ppp_ops_start)
    opt CONFIG_NET_CONNECT_PPP_DEVICE_USB
        PPP->>IDF: ppp_usb_install() -> tinyusb_driver_install(), tinyusb_cdcacm_init(cdc_rx_callback)
        IDF-->>PPP: Return ESP_OK
        PPP->>IDF: tinyusb_cdcacm_register_callback(CDC_EVENT_LINE_STATE_CHANGED, line_state_changed)
        IDF-->>PPP: Return ESP_OK
    end
    PPP->>IDF: esp_netif_new() with ESP_NETIF_DEFAULT_PPP and s_ppp_driver_cfg (.transmit = transmit)
    IDF-->>PPP: Return s_netif
    PPP->>Loop: Register (IP_EVENT, IP_EVENT_PPP_LOST_IP) -> on_ppp_lost_ip
    Loop-->>PPP: Return ESP_OK
    PPP->>Core: net_iface_notify_started(s_netif)
    Core->>Core: Save netif, state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_STARTED
    Loop-->>Core: Queued
    Core-->>PPP: Return from net_iface_notify_started()
    PPP-->>Task: Return ESP_OK from ppp_ops_start()
    end

    rect 4. Auto-connect and start PPP negotiation (STARTED -> CONNECTING -> CONNECTED)
    Note over Task,Core: auto_connect_on_start is true and stop was not requested
    Task->>Core: net_iface_connect()
    Core->>Core: Clear READY/FAILED bits, state = CONNECTING
    Core->>PPP: ops->connect() (ppp_ops_connect)
    PPP->>PPP: retry_num = 0
    alt CONFIG_NET_CONNECT_PPP_DEVICE_USB
        PPP->>IDF: esp_netif_action_start(s_netif) and esp_netif_action_connected(s_netif)
        IDF-->>PPP: Return
    else CONFIG_NET_CONNECT_PPP_DEVICE_UART
        PPP->>PPP: Create/clear s_ppp_exit_sem, s_stop_task = false
        PPP->>Worker: xTaskCreate(ppp_task, "ppp connect")
        Worker->>IDF: uart_driver_install(UART_NUM_1), uart_param_config(), uart_set_pin(), uart_set_rx_timeout()
        IDF-->>Worker: Return ESP_OK
        Worker->>Loop: Register (IP_EVENT, IP_EVENT_PPP_GOT_IP) -> esp_netif_action_connected
        Loop-->>Worker: Return ESP_OK
        Worker->>IDF: esp_netif_action_start(s_netif) and esp_netif_action_connected(s_netif)
        IDF-->>Worker: Return
        Note over Worker,IDF: Worker loops on event_queue -> uart_read_bytes() -> esp_netif_receive(s_netif)
    end
    PPP->>Core: net_iface_notify_connected(self)
    Core->>Core: state = CONNECTED, retry_num = 0
    opt IPv6 enabled
        Core->>IDF: esp_netif_create_ip6_linklocal()
        IDF-->>Core: Return
    end
    Core->>Loop: Post NET_IFACE_EVENT_CONNECTED
    Loop-->>Core: Queued
    Core-->>PPP: Return from net_iface_notify_connected()
    PPP-->>Core: Return ESP_OK from ppp_ops_connect()
    Core-->>Task: Return ESP_OK from net_iface_connect()
    Task->>Core: Save stack HWM, set STARTUP_DONE
    Core-->>Task: Done
    Task->>Task: vTaskDelete(NULL)
    end

    rect 5. IPCP address acquired (CONNECTED -> READY)
    IDF->>Loop: Post IP_EVENT_PPP_GOT_IP (and IP_EVENT_GOT_IP6 if required)
    Loop->>Core: on_global_ip_event()
    Core->>Core: Set IPV4 / IPV6 bit, check is_ready_satisfied_locked()
    Core->>Core: net_iface_notify_ready() -> state = READY, set READY bit, pulse global wait
    Core->>Loop: Post NET_IFACE_EVENT_READY
    Loop-->>Core: Queued
    Core-->>Loop: Return from on_global_ip_event()
    end

    rect 6. Wait for readiness (READY)
    App->>Core: net_iface_wait(timeout)
    Core-->>App: READY bit is set -> return ESP_OK
    end
```

### Flow 7: disconnect, stop, and destroy

`net_iface_disconnect()` stops the UART worker task (in UART mode) and stops PPP negotiation (`esp_netif_action_disconnected()` + `esp_netif_action_stop()`) while keeping `s_netif` alive in `STARTED`. `net_iface_stop()` unregisters `on_ppp_lost_ip`, destroys `s_netif`, and uninstalls TinyUSB (in USB mode).

```mermaid
sequenceDiagram
    participant App as User Application
    participant PPP as PPP (ppp_connect.c)
    participant Core as net_iface Core
    participant Worker as UART Task (ppp_task)
    participant IDF as ESP-IDF PPP netif / Transport
    participant Loop as Default Event Loop

    rect 1. Disconnect (READY/CONNECTED -> STARTED)
    App->>Core: net_iface_disconnect()
    Core->>PPP: ops->disconnect() (ppp_ops_disconnect)
    opt CONFIG_NET_CONNECT_PPP_DEVICE_UART
        PPP->>PPP: ppp_stop_uart_task() -> s_stop_task = true, wait on s_ppp_exit_sem (up to 1 s)
        Worker->>Loop: Unregister (IP_EVENT, IP_EVENT_PPP_GOT_IP) -> esp_netif_action_connected
        Loop-->>Worker: Return ESP_OK
        Worker->>IDF: uart_driver_delete(UART_NUM_1), free(buffer)
        IDF-->>Worker: Return
        Worker->>PPP: xSemaphoreGive(s_ppp_exit_sem) (ppp_signal_task_exited)
        Worker->>Worker: vTaskDelete(NULL)
        PPP->>PPP: vSemaphoreDelete(s_ppp_exit_sem)
    end
    PPP->>IDF: esp_netif_action_disconnected(s_netif) and esp_netif_action_stop(s_netif)
    IDF-->>PPP: Return
    PPP-->>Core: Return ESP_OK from ppp_ops_disconnect()
    Core->>Core: net_iface_notify_disconnected() -> state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_DISCONNECTED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK (s_netif stays allocated)
    end

    rect 2. Stop driver (STARTED -> STOPPING -> INIT)
    App->>Core: net_iface_stop()
    Core->>Core: state = STOPPING (wait for STARTUP_DONE if startup task is running)
    Core->>PPP: ops->disconnect() (ppp_ops_disconnect)
    PPP-->>Core: Return ESP_OK
    Core->>PPP: ops->stop() (ppp_ops_stop)
    PPP->>Loop: Unregister (IP_EVENT, IP_EVENT_PPP_LOST_IP) -> on_ppp_lost_ip
    Loop-->>PPP: Return ESP_OK
    opt CONFIG_NET_CONNECT_PPP_DEVICE_UART
        PPP->>PPP: ppp_stop_uart_task() (if still running)
    end
    PPP->>IDF: esp_netif_action_disconnected(netif), esp_netif_action_stop(netif), vTaskDelay(100 ms), esp_netif_destroy(netif)
    IDF-->>PPP: Return
    opt CONFIG_NET_CONNECT_PPP_DEVICE_USB
        PPP->>IDF: ppp_usb_uninstall() -> unregister line_state_changed, tinyusb_driver_uninstall()
        IDF-->>PPP: Return
    end
    PPP-->>Core: Return ESP_OK from ppp_ops_stop()
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
    Note over Core,PPP: ops->deinit is NULL for PPP
    Core->>Core: Free event group, mutex, and handle (state = UNINIT)
    Core-->>App: Return ESP_OK
    end
```

### Flow 6: PPP address loss, retry, and `FAILED`

When `IP_EVENT_PPP_LOST_IP` fires, both `on_global_ip_event()` (in `net_iface.c`) and `on_ppp_lost_ip()` (in `ppp_connect.c`) run on the default event loop. `on_global_ip_event()` moves the state back to `CONNECTING`, and `on_ppp_lost_ip()` either restarts PPP negotiation (`retry_num <= max_retry`) or marks the handle `FAILED` (`retry_num > max_retry`).

```mermaid
sequenceDiagram
    participant App as User Application
    participant PPP as PPP (ppp_connect.c)
    participant Core as net_iface Core
    participant Worker as UART Task (ppp_task)
    participant IDF as ESP-IDF PPP netif / Transport
    participant Loop as Default Event Loop

    rect Flow 6a - PPP lost IP, retry or fail (READY/CONNECTED -> CONNECTING or FAILED)
    IDF->>Loop: 1. Post IP_EVENT_PPP_LOST_IP
    Loop->>Core: on_global_ip_event(IP_EVENT_PPP_LOST_IP)
    Core->>Core: net_iface_notify_lost_ip() -> clear READY and IPV4 bits, state = CONNECTING
    Core->>Loop: Post NET_IFACE_EVENT_LOST_IP
    Loop-->>Core: Queued
    Core-->>Loop: Return from on_global_ip_event()
    Loop->>PPP: on_ppp_lost_ip(IP_EVENT_PPP_LOST_IP)
    PPP->>PPP: Increment retry_num under self->lock
    alt retry_num <= max_retry
        PPP->>IDF: esp_netif_action_start(s_netif) and esp_netif_action_connected(s_netif)
        IDF-->>PPP: Return
        PPP-->>Loop: Return from on_ppp_lost_ip()
    else retry_num > max_retry
        PPP->>Core: net_iface_notify_failed(self, ESP_FAIL)
        Core->>Core: state = FAILED, set FAILED bit, wake waiters
        Core->>Loop: Post NET_IFACE_EVENT_FAILED
        Loop-->>Core: Queued
        Core-->>PPP: Return from net_iface_notify_failed()
        PPP-->>Loop: Return from on_ppp_lost_ip()
    end
    end

    rect Flow 6b - UART setup failure inside ppp_task (CONNECTED -> FAILED)
    Worker->>IDF: 2. uart_driver_install(), uart_param_config(), uart_set_pin(), or malloc() fails
    IDF-->>Worker: Return error
    Worker->>IDF: uart_driver_delete(UART_NUM_1) if installed
    IDF-->>Worker: Return
    Worker->>Core: net_iface_notify_failed(self, err)
    Core->>Core: state = FAILED, set FAILED bit, wake waiters
    Core->>Loop: Post NET_IFACE_EVENT_FAILED
    Loop-->>Core: Queued
    Core-->>Worker: Return from net_iface_notify_failed()
    Worker->>PPP: ppp_signal_task_exited() (give s_ppp_exit_sem)
    Worker->>Worker: vTaskDelete(NULL)
    end
```

---

## The objects the driver keeps

PPP fills the ops table in `ppp_connect.c` (lines 460–468):

```c
const net_iface_ops_t g_net_iface_ppp_ops = {
    .name = "ppp",
    .init = NULL,
    .deinit = NULL,
    .start = ppp_ops_start,
    .stop = ppp_ops_stop,
    .connect = ppp_ops_connect,
    .disconnect = ppp_ops_disconnect,
};
```

Both transports share a single `esp_netif_driver_ifconfig_t` (`ppp_connect.c`, lines 75–78):

```c
static esp_netif_driver_ifconfig_t s_ppp_driver_cfg = {
    .handle = (void *)1,
    .transmit = transmit,
};
```

- `transmit()` (`ppp_connect.c`, lines 58–69) writes outgoing PPP frames using `tinyusb_cdcacm_write_queue()` + `tinyusb_cdcacm_write_flush()` in USB mode, or `uart_write_bytes(UART_NUM_1, ...)` in UART mode.
- Incoming PPP bytes are fed into `esp_netif_receive(s_netif, ...)` from `cdc_rx_callback()` (USB mode, lines 82–96) or from the `while (!s_stop_task)` loop inside `ppp_task()` (UART mode, lines 228–245).

---

## Flow 1: Create the handle

Public call: `net_ppp_create()` in `ppp_connect.c` (lines 470–515):

1. Builds `net_iface_config_t`:
   - `type = NET_IFACE_TYPE_PPP`
   - `desc = NET_CONNECT_NETIF_DESC_PPP` (`"net_connect_netif_ppp"`)
   - `route_prio = NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO` (32)
   - `required` from `CONFIG_NET_CONNECT_PPP_REQUIRED`
   - `auto_connect_on_start = true`
   - `ip.use_dhcp = true`, `ip.enable_ipv6` from `CONFIG_NET_CONNECT_IPV6`
   - `ppp.max_retry = CONFIG_NET_CONNECT_PPP_CONN_MAX_RETRY`
   - `ppp.task_stack_size = CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE`
2. A non-NULL `net_ppp_params_t` overrides `max_retry`, `task_stack_size`, `route_prio`, `required`, `auto_connect_on_start`, and `ip`.
3. Calls `net_iface_init_with_ops(&cfg, &g_net_iface_ppp_ops, out_handle)`.

---

## Flow 2: `net_iface_start()`, `ppp_ops_start()`, and `ppp_ops_connect()`

`net_iface_start()` spawns `iface_startup_task`, which calls `ppp_ops_start()` (`ppp_connect.c`, lines 320–365):

1. Rejects a second start if `s_netif != NULL` (`ESP_ERR_INVALID_STATE`).
2. In USB mode (`CONFIG_NET_CONNECT_PPP_DEVICE_USB`), calls `ppp_usb_install()` (lines 107–141) to install TinyUSB, initialize `TINYUSB_CDC_ACM_0` with `cdc_rx_callback`, and register `line_state_changed` on `CDC_EVENT_LINE_STATE_CHANGED`.
3. Creates `s_netif` using `ESP_NETIF_INHERENT_DEFAULT_PPP()`, `s_ppp_driver_cfg`, and `ESP_NETIF_NETSTACK_DEFAULT_PPP`.
4. Registers `(IP_EVENT, IP_EVENT_PPP_LOST_IP)` → `on_ppp_lost_ip` with `self`.
5. Calls `net_iface_notify_started(self, s_netif)` (`STARTING` → `STARTED`).

If `auto_connect_on_start` is true and `net_iface_stop()` was not requested, `iface_startup_task` calls `net_iface_connect(h)` → `ppp_ops_connect()` (`ppp_connect.c`, lines 368–413):

- Resets `self->retry_num = 0` under `self->lock`.
- **In USB mode:** Calls `esp_netif_action_start(s_netif, 0, 0, 0)` and `esp_netif_action_connected(s_netif, 0, 0, 0)`, then calls `net_iface_notify_connected(self)`.
- **In UART mode:** Creates `s_ppp_exit_sem`, spawns `ppp_task` (`"ppp connect"`), sets `s_ppp_task_published = true`, and calls `net_iface_notify_connected(self)`. Inside `ppp_task()` (lines 172–252), the task installs `UART_NUM_1`, configures baud rate / pins / RX timeout, allocates a 1024-byte RX buffer, registers `(IP_EVENT, IP_EVENT_PPP_GOT_IP)` → `esp_netif_action_connected`, calls `esp_netif_action_start()` and `esp_netif_action_connected()`, and loops reading UART frames into `esp_netif_receive()`.

---

## Flow 3: IPCP address and `READY`

When LCP/IPCP negotiation completes with the peer, lwIP posts `IP_EVENT_PPP_GOT_IP` on the default event loop:

1. `on_global_ip_event()` (`net_iface.c`, lines 673–678) matches `event->esp_netif` to the PPP handle and sets `NET_IFACE_BIT_IPV4` (and `IP_EVENT_GOT_IP6` sets `NET_IFACE_BIT_IPV6` when IPv6 is enabled and a preferred address arrives).
2. Once `is_ready_satisfied_locked()` passes, `net_iface_notify_ready()` transitions `CONNECTED` → `READY`, sets `NET_IFACE_BIT_READY`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_READY`.

---

## Flow 6: Address loss, retry, and `FAILED`

When the PPP session drops or negotiation times out, `IP_EVENT_PPP_LOST_IP` fires on the default event loop:

1. `on_global_ip_event()` calls `net_iface_notify_lost_ip()`, which transitions `READY` or `CONNECTED` back to `CONNECTING`, clears `NET_IFACE_BIT_READY` and `NET_IFACE_BIT_IPV4`, and posts `NET_IFACE_EVENT_LOST_IP`.
2. `on_ppp_lost_ip()` (`ppp_connect.c`, lines 280–317) checks that `event->esp_netif == s_netif` and that the handle is not in `STOPPING`, `INIT`, or `STARTED`.
3. Increments `self->retry_num` under `self->lock`:
   - If `retry_num <= max_retry`, restarts PPP negotiation with `esp_netif_action_start(s_netif, 0, 0, 0)` and `esp_netif_action_connected(s_netif, 0, 0, 0)`.
   - If `retry_num > max_retry`, calls `net_iface_notify_failed(self, ESP_FAIL)`, transitioning the handle to `FAILED` and waking any waiting tasks.

---

## Flow 7: Disconnect, stop, and destroy

- **Stop PPP negotiation only (`net_iface_disconnect`):** `ppp_ops_disconnect()` (`ppp_connect.c`, lines 416–427) stops `ppp_task` via `ppp_stop_uart_task()` (in UART mode), then calls `esp_netif_action_disconnected(s_netif, 0, 0, 0)` and `esp_netif_action_stop(s_netif, 0, 0, 0)`. `net_iface_notify_disconnected()` moves the handle back to `STARTED`. `s_netif` (and TinyUSB in USB mode) stays up so `net_iface_connect()` can start a new session.
- **Stop the driver (`net_iface_stop`):** `ppp_ops_stop()` (`ppp_connect.c`, lines 430–457):
  1. Unregisters `on_ppp_lost_ip` (`IP_EVENT_PPP_LOST_IP`).
  2. In UART mode, calls `ppp_stop_uart_task()` (which sets `s_stop_task = true` and waits up to 1000 ms on `s_ppp_exit_sem` for `ppp_task` to unregister `IP_EVENT_PPP_GOT_IP`, call `uart_driver_delete(UART_NUM_1)`, free its buffer, and exit).
  3. Clears `s_netif = NULL` before teardown so a late USB RX callback cannot dereference a freed netif, then calls `esp_netif_action_disconnected()`, `esp_netif_action_stop()`, delays 100 ms, and calls `esp_netif_destroy()`.
  4. In USB mode, calls `ppp_usb_uninstall()`.
  `net_iface_notify_stopped()` then resets the handle to `INIT`, sets `NET_IFACE_BIT_STOPPED`, and posts `NET_IFACE_EVENT_STOPPED`.
- **Free the handle (`net_iface_deinit`):** Unlinks the handle in `INIT`, unregisters the shared `NET_IFACE_EVENT` and `IP_EVENT` handlers when the registry is empty, and frees the handle.
