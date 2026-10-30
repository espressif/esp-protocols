# API reference by layer

Every interface function in `components/net_connect`, grouped by layer and listed in the order a typical lifecycle calls them.

---

## 1. High-level orchestrator (`connect.c`, `stdin_out.c`, `console_cmd.c`)

These are the top-level helpers declared in `include/net_connect.h` and `include/net_connect_private.h`. An application that wants one call to bring up all Kconfig-enabled interfaces uses this layer.

### 1.1 `net_configure_stdin_stdout()`
```c
esp_err_t net_configure_stdin_stdout(void);
```
- **Arguments:** None.
- **What it does:** Installs the UART driver on `CONFIG_ESP_CONSOLE_UART_NUM` (if not already installed), switches `stdin`/`stdout` to use that UART driver with unbuffered input, and sets CR/CRLF line endings.
- **Capabilities:**
  - Enables blocking standard C/C++ console I/O (`fgets`, `scanf`, `printf`, `std::cin`) over UART.
  - Safe to call more than once (returns `ESP_OK` immediately if the UART driver is already installed).

### 1.2 `net_register_wifi_connect_commands()`
```c
void net_register_wifi_connect_commands(void);
```
- **Arguments:** None.
- **What it does:** Finds an existing Wi-Fi STA handle or creates an idle one (`ssid = ""`, `auto_connect_on_start = false`), starts the Wi-Fi radio, waits until startup finishes (`NET_IFACE_BIT_STARTUP_DONE`), and registers the `wifi_connect` and `wifi_disconnect` commands with `esp_console`.
- **Capabilities:**
  - Brings the Wi-Fi radio up in `STARTED` state without joining an AP until the user enters a command.
  - `wifi_connect <ssid> [pass] [-n channel]` disconnects any active link, stores the new credentials, starts connecting, and optionally locks the Wi-Fi channel.
  - `wifi_disconnect` drops the current AP association while leaving the radio running for future `wifi_connect` commands.

### 1.3 `net_connect()`
```c
esp_err_t net_connect(void);
```
- **Arguments:** None.
- **What it does:** Creates a handle for each Kconfig-enabled interface (`ETH`, `WIFI_STA`, `THREAD`, `PPP`) if one does not already exist, starts them all in parallel, registers a restart shutdown handler, waits for the required interfaces (or at least one optional interface) to become `READY`, and prints assigned IP addresses.
- **Capabilities:**
  - Starts all enabled interfaces in parallel rather than one after another.
  - Reuses any handle the application created or configured beforehand.
  - Enforces required vs. optional rules: fails and tears down all interfaces if a `required` interface fails or times out, but keeps working interfaces up if an `optional` interface fails or has no credentials.
  - Automatically stops all active interfaces on `esp_restart()`.

### 1.4 `net_get_netif()` / `NET_CONNECT_INTERFACE`
```c
#define net_get_netif() net_get_netif_from_desc(...)
```
- **Arguments:** None.
- **What it does:** Returns the default `esp_netif_t *` created by `net_connect()`, preferring Ethernet first, then Wi-Fi, then Thread, then PPP.
- **Capabilities:**
  - Gives single-interface applications direct access to the active `esp_netif_t *` without needing to know its description string.

### 1.5 `net_get_netif_from_desc()`
```c
esp_netif_t *net_get_netif_from_desc(const char *desc);
```
- **Arguments:**
  - `desc`: Exact netif description string (for example `"net_connect_netif_sta"` or `"net_connect_netif_eth"`).
- **What it does:** Searches ESP-IDF's netif list and returns the `esp_netif_t *` whose description matches `desc`, or `NULL`.
- **Capabilities:**
  - Finds a specific `esp_netif_t *` by name when multiple interfaces are running at the same time.
  - Returns `NULL` safely if `desc` is `NULL`, empty, or not found.

### 1.6 `net_connect_is_our_netif()` *(component-private)*
```c
bool net_connect_is_our_netif(const char *prefix, esp_netif_t *netif);
```
- **Arguments:**
  - `prefix`: Description prefix to match.
  - `netif`: Pointer to an `esp_netif_t`.
- **What it does:** Returns `true` if `netif`'s description starts with `prefix`.
- **Capabilities:**
  - Distinguishes `esp_netif` objects created by `net_connect` from other `esp_netif` objects created by the application.

### 1.7 `net_connect_print_all_netif_ips()` *(component-private)*
```c
void net_connect_print_all_netif_ips(const char *prefix);
```
- **Arguments:**
  - `prefix`: Description prefix of the interfaces to print.
- **What it does:** Runs a callback on the TCP/IP task via `esp_netif_tcpip_exec()` and logs the IPv4 and IPv6 addresses of every matching `esp_netif`.
- **Capabilities:**
  - Safely walks the global `esp_netif` list on the lwIP TCP/IP thread without racing concurrent netif creation or destruction.

### 1.8 `net_disconnect()`
```c
esp_err_t net_disconnect(void);
```
- **Arguments:** None.
- **What it does:** Unregisters the restart shutdown handler, stops every registered interface (`net_iface_stop()`), and frees every handle (`net_iface_deinit()`).
- **Capabilities:**
  - Stops all drivers, destroys all `esp_netif` instances, and frees all handle and registry memory in one call.

---

## 2. Core interface manager (`net_iface.c`)

Declared in `include/net_iface.h` (public) and `include/net_iface_priv.h` (driver-facing). This layer owns the handle registry, state machine, background startup task, event notifications, and blocking waits.

### 2.1 Public lifecycle and wait APIs (in call order)

#### 1. `net_iface_init()`
```c
esp_err_t net_iface_init(const net_iface_config_t *cfg, net_iface_handle_t *out_handle);
```
- **Arguments:**
  - `cfg`: Full configuration struct (`type`, `desc`, `route_prio`, `required`, `auto_connect_on_start`, `startup_task_stack_size`, `ip`, and driver union).
  - `out_handle`: Output pointer that receives the new handle.
- **What it does:** Picks the driver ops table for `cfg->type` and calls `net_iface_init_with_ops()` to allocate the handle in `NET_IFACE_STATE_INIT`.
- **Capabilities:**
  - Creates the handle in `INIT` state without touching any radio, PHY, UART, or USB hardware.
  - Can be called before or after `esp_event_loop_create_default()`.
  - Returns `ESP_ERR_NOT_SUPPORTED` if the requested interface type is disabled in Kconfig.

#### 2. `net_iface_register_event_handler()`
```c
esp_err_t net_iface_register_event_handler(esp_event_handler_t handler, void *arg);
```
- **Arguments:**
  - `handler`: Callback function for `NET_IFACE_EVENT`.
  - `arg`: User context pointer passed to `handler`.
- **What it does:** Subscribes `handler` to all `NET_IFACE_EVENT` notifications (`STARTED`, `CONNECTED`, `READY`, `LOST_IP`, `DISCONNECTED`, `STOPPED`, `FAILED`) on the default event loop.
- **Capabilities:**
  - Delivers every interface state transition asynchronously on the default event loop with a `net_iface_event_data_t` payload (`iface`, `netif`, `type`, `state`, `err`).
  - Does not replay events that happened before registration; callers can read `net_iface_get_state()` right after registering.

#### 3. `net_iface_start()`
```c
esp_err_t net_iface_start(net_iface_handle_t h);
```
- **Arguments:**
  - `h`: Interface handle (must be in `INIT` or `FAILED`).
- **What it does:** Clears old status bits, moves the state to `STARTING`, spawns `iface_startup_task` to run `ops->start(h)` (and `net_iface_connect(h)` if `auto_connect_on_start` is true), and returns `ESP_OK` immediately.
- **Capabilities:**
  - Returns immediately without blocking on slow hardware init (such as Wi-Fi RF init or Ethernet PHY setup).
  - Can be called from `INIT` (first start) or `FAILED` (to retry after a failed startup).
  - Allows multiple interfaces to initialize their drivers concurrently in separate background tasks.

#### 4. `net_iface_connect()`
```c
esp_err_t net_iface_connect(net_iface_handle_t h);
```
- **Arguments:**
  - `h`: Interface handle (must be in `STARTED` or `FAILED`).
- **What it does:** Clears `READY` and `FAILED` bits, sets the state to `CONNECTING`, and calls `ops->connect(h)` to start joining/negotiating.
- **Capabilities:**
  - Returns immediately after kicking off association or link negotiation.
  - Can be called from `STARTED` (after manual provisioning or `net_iface_disconnect()`) or from `FAILED` (to retry connecting without restarting the driver).
  - If `ops->connect(h)` returns `ESP_ERR_INVALID_STATE` (for example, Wi-Fi SSID is still empty), puts the state back to `STARTED` instead of marking the interface `FAILED`.

#### 5. `net_iface_wait()`
```c
esp_err_t net_iface_wait(net_iface_handle_t h, uint32_t timeout_ms);
```
- **Arguments:**
  - `h`: Interface handle.
  - `timeout_ms`: Maximum time to wait in milliseconds (`UINT32_MAX` waits forever).
- **What it does:** Sleeps on the handle's sticky `READY | FAILED | STOPPED` event bits until one is set or the timeout expires.
- **Capabilities:**
  - Returns `ESP_OK` immediately if the interface is already `READY`, or returns `h->last_err` immediately if it is already `FAILED`.
  - Allows multiple FreeRTOS tasks to wait on the same handle at the same time and wakes all of them.
  - Protects against deadlocks by returning `ESP_ERR_INVALID_STATE` if called from the default event loop task, or if the handle is still in `INIT` / `UNINIT`.
  - On `ESP_ERR_TIMEOUT`, leaves the background connection attempt running without stopping the interface.

#### 6. `net_iface_wait_all()`
```c
esp_err_t net_iface_wait_all(const net_iface_handle_t *handles, size_t count, uint32_t timeout_ms);
```
- **Arguments:**
  - `handles`: Array of interface handles.
  - `count`: Number of handles in the array.
  - `timeout_ms`: Shared timeout budget in milliseconds across all handles.
- **What it does:** Waits until **every** handle in `handles` is `READY`.
- **Capabilities:**
  - Shares one overall timeout deadline across all handles in the array.
  - Fails fast and returns immediately as soon as any handle in the array enters `FAILED` or `STOPPED`.
  - Refuses to run on the default event loop task (`ESP_ERR_INVALID_STATE`).

#### 7. `net_iface_wait_any()`
```c
esp_err_t net_iface_wait_any(const net_iface_handle_t *handles, size_t count,
                             uint32_t timeout_ms, net_iface_handle_t *out_ready);
```
- **Arguments:**
  - `handles`: Array of interface handles.
  - `count`: Number of handles in the array.
  - `timeout_ms`: Shared timeout budget in milliseconds.
  - `out_ready`: Optional output pointer that receives the first handle that became `READY` (can be `NULL`).
- **What it does:** Waits until **at least one** handle in `handles` is `READY`.
- **Capabilities:**
  - Returns `ESP_OK` (and writes `*out_ready`) as soon as the first interface reaches `READY`.
  - Keeps waiting if one interface fails while another interface in the list is still trying.
  - Fails fast only when **every** handle in the list has entered `FAILED` or `STOPPED`.

#### 8. Inspection and lookup getters
```c
esp_netif_t       *net_iface_get_netif(net_iface_handle_t h);
net_iface_state_t  net_iface_get_state(net_iface_handle_t h);
net_iface_type_t   net_iface_get_type(net_iface_handle_t h);
esp_err_t          net_iface_get_last_error(net_iface_handle_t h);
net_iface_handle_t net_iface_get_by_type(net_iface_type_t type);
net_iface_handle_t net_iface_get_by_desc(const char *desc);
esp_err_t          net_iface_for_each(net_iface_cb_t cb, void *ctx);
```
- **Arguments:** Handle `h`, interface `type`, description string `desc`, or callback `cb` + `ctx`.
- **What they do:**
  - `net_iface_get_netif`: Returns the underlying `esp_netif_t *` (or `NULL` if not started).
  - `net_iface_get_state`: Returns the current `net_iface_state_t` (`UNINIT` if `h` is `NULL`).
  - `net_iface_get_type`: Returns the `net_iface_type_t` of `h`.
  - `net_iface_get_last_error`: Returns the `esp_err_t` stored when `h` entered `FAILED`.
  - `net_iface_get_by_type`: Returns the first registered handle matching `type`, or `NULL`.
  - `net_iface_get_by_desc`: Returns the registered handle whose `cfg.desc` matches `desc`, or `NULL`.
  - `net_iface_for_each`: Walks all registered handles and calls `cb(h, ctx)` for each one.
- **Capabilities:**
  - Thread-safe: all getters acquire the necessary internal locks, so they can be called safely from any task or from inside a `NET_IFACE_EVENT` callback.
  - `net_iface_for_each()` copies the handle list before releasing the list lock, so `cb` can safely call `net_iface_get_state()`, `net_iface_stop()`, or other APIs without deadlocking.

#### 9. `net_iface_disconnect()`
```c
esp_err_t net_iface_disconnect(net_iface_handle_t h);
```
- **Arguments:**
  - `h`: Interface handle (must be in `CONNECTING`, `CONNECTED`, `READY`, or `FAILED`).
- **What it does:** Calls `ops->disconnect(h)` to drop the link, clears liveness/IP bits, sets the state back to `STARTED`, and posts `NET_IFACE_EVENT_DISCONNECTED`.
- **Capabilities:**
  - Drops the L2 connection while keeping the driver and `esp_netif` initialized in `STARTED`, so a later `net_iface_connect(h)` can reconnect without re-initializing hardware.

#### 10. `net_iface_stop()`
```c
esp_err_t net_iface_stop(net_iface_handle_t h);
```
- **Arguments:**
  - `h`: Interface handle.
- **What it does:** Sets `cancel_requested = true` and `state = STOPPING`, waits up to 10 s for `iface_startup_task` to finish if it is still running, calls `ops->disconnect(h)` and `ops->stop(h)`, resets the state to `INIT`, sets `NET_IFACE_BIT_STOPPED`, and posts `NET_IFACE_EVENT_STOPPED`.
- **Capabilities:**
  - Safe to call in `STARTING`, `STARTED`, `CONNECTING`, `CONNECTED`, `READY`, or `FAILED` (and a no-op returning `ESP_OK` if already in `INIT` or `STOPPING`).
  - Safely cancels an in-flight startup by waiting for `iface_startup_task` to finish `ops->start()` before tearing down resources.
  - Always transitions the handle back to `INIT` (even if `ops->stop()` returns an error) and wakes any tasks blocked in `net_iface_wait*()`.

#### 11. `net_iface_unregister_event_handler()`
```c
esp_err_t net_iface_unregister_event_handler(esp_event_handler_t handler);
```
- **Arguments:**
  - `handler`: Previously registered `NET_IFACE_EVENT` callback.
- **What it does:** Removes `handler` from the default event loop.
- **Capabilities:**
  - Stops `handler` from receiving future `NET_IFACE_EVENT` notifications.

#### 12. `net_iface_deinit()`
```c
esp_err_t net_iface_deinit(net_iface_handle_t h);
```
- **Arguments:**
  - `h`: Interface handle (must be in `INIT`).
- **What it does:** Removes `h` from the global registry, unregisters global `NET_IFACE_EVENT` / `IP_EVENT` / `OPENTHREAD_EVENT` handlers if the registry is now empty, calls `ops->deinit(h)`, and frees the mutex, event group, and handle memory.
- **Capabilities:**
  - Rejects running handles (`ESP_ERR_INVALID_STATE`) so a handle cannot be freed while its driver or tasks are still active.
  - Automatically unregisters shared event listeners and deletes global locks when the last handle is freed, returning heap usage to baseline.

---

### 2.2 Internal driver-to-core APIs (`include/net_iface_priv.h`)

Drivers (`wifi_connect.c`, `eth_connect.c`, `thread_connect.c`, `ppp_connect.c`) call these functions to report progress to `net_iface.c`:

1. **`net_iface_init_with_ops(cfg, ops, out_handle)`**
   - **What it does:** Allocates `struct net_iface`, attaches `ops`, calls `ops->init` if non-NULL, inserts the handle into `s_iface_list`, and tries to register global event handlers.
   - **Capabilities:** Allows any driver (or unit-test mock driver) to plug custom `net_iface_ops_t` callbacks into the state machine.
2. **`net_iface_notify_started(h, netif)`**
   - **What it does:** Stores `h->netif`, moves the state to `STARTED`, clears `NET_IFACE_BIT_STOPPED`, and posts `NET_IFACE_EVENT_STARTED`.
   - **Capabilities:** If `h` is already `STOPPING`, still saves `h->netif` without entering `STARTED` so `ops->stop()` can free the netif.
3. **`net_iface_notify_connected(h)`**
   - **What it does:** Moves the state to `CONNECTED`, resets `retry_num = 0`, calls `esp_netif_create_ip6_linklocal()` when IPv6 is enabled (non-Thread), and posts `NET_IFACE_EVENT_CONNECTED`.
   - **Capabilities:** Ignored if `h` is `STOPPING`, `INIT`, or already `CONNECTED` / `READY`.
4. **`net_iface_notify_ready(h)`**
   - **What it does:** Moves the state to `READY`, clears `FAILED` and `STOPPED`, sets `NET_IFACE_BIT_READY`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_READY`.
   - **Capabilities:** Wakes both single-handle waiters (`net_iface_wait`) and multi-handle waiters (`wait_all` / `wait_any`).
5. **`net_iface_notify_lost_ip(h)`**
   - **What it does:** Moves `READY` or `CONNECTED` back to `CONNECTING`, clears `READY` and `IPV4` bits, and posts `NET_IFACE_EVENT_LOST_IP`.
   - **Capabilities:** Marks the interface no longer ready without pulsing group waiters or stopping the driver.
6. **`net_iface_notify_disconnected(h)`**
   - **What it does:** Moves the state back to `STARTED`, clears all liveness bits, and posts `NET_IFACE_EVENT_DISCONNECTED`.
   - **Capabilities:** Resets readiness while keeping the handle in `STARTED` for a future `net_iface_connect()`.
7. **`net_iface_notify_failed(h, err)`**
   - **What it does:** Saves `err` in `h->last_err` (converting `ESP_OK` to `ESP_FAIL`), sets `FAILED`, clears `READY`, sets `NET_IFACE_BIT_FAILED`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_FAILED`.
   - **Capabilities:** Wakes any task blocked in `net_iface_wait*()` with the saved error code.
8. **`net_iface_notify_stopped(h)`**
   - **What it does:** Clears `h->netif`, moves the state to `INIT`, clears liveness bits, sets `NET_IFACE_BIT_STOPPED`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_STOPPED`.
   - **Capabilities:** Returns the handle to `INIT` so `net_iface_start()` or `net_iface_deinit()` can be called next.

---

## 3. Wi-Fi STA layer (`wifi_connect.c`)

Declared in `include/net_iface.h`, `include/net_connect_wifi_config.h`, and `include/net_connect_private.h`.

### 3.1 Handle-based Wi-Fi APIs (in call order)

#### 1. `net_wifi_sta_create()`
```c
esp_err_t net_wifi_sta_create(const net_wifi_sta_params_t *params, net_iface_handle_t *out_handle);
```
- **Arguments:**
  - `params`: Optional Wi-Fi parameters (`ssid`, `password`, `max_retry`, `route_prio`, `required`, `auto_connect_on_start`, `ip`), or `NULL` for Kconfig defaults.
  - `out_handle`: Output pointer receiving the new Wi-Fi STA handle.
- **What it does:** Fills a `net_iface_config_t` from Kconfig defaults, applies any overrides from `params`, disables `auto_connect_on_start` if the SSID is empty, and calls `net_iface_init_with_ops(&cfg, &g_net_iface_wifi_sta_ops, out_handle)`.
- **Capabilities:**
  - Creates the Wi-Fi STA handle in `INIT` state without initializing or starting the Wi-Fi radio.
  - Supports both pre-configured credentials and unprovisioned startup (passing `ssid = ""` keeps the station in `STARTED` when started, waiting for `net_wifi_sta_set_credentials()`).
  - Returns `ESP_ERR_NOT_SUPPORTED` when Wi-Fi is disabled in Kconfig.

#### 2. `net_wifi_sta_set_credentials()`
```c
esp_err_t net_wifi_sta_set_credentials(net_iface_handle_t h, const char *ssid, const char *password);
```
- **Arguments:**
  - `h`: Wi-Fi STA handle (must be in `INIT`, `STARTED`, or `FAILED`).
  - `ssid`: Non-empty AP SSID string.
  - `password`: AP password string (`NULL` or `""` for an open network).
- **What it does:** Copies `ssid` and `password` into `h->cfg.wifi_sta` under `h->lock`.
- **Capabilities:**
  - Works before the radio is started (`INIT`), while the radio is idle (`STARTED`), or after a failed connection attempt (`FAILED`).
  - Does not touch the Wi-Fi driver directly; the stored credentials are applied to the radio on the next `net_iface_connect(h)` call.
  - Rejects changes while an association is active or in progress (`CONNECTING`, `CONNECTED`, `READY`) with `ESP_ERR_INVALID_STATE`.

#### 3. `net_connect_wifi_apply_stdin()` *(component-private)*
```c
esp_err_t net_connect_wifi_apply_stdin(net_iface_handle_t h);
```
- **Arguments:**
  - `h`: Wi-Fi STA handle.
- **What it does:** Configures `stdin`/`stdout`, blocks waiting for a `"ssid password"` line from the serial console, stores the credentials via `net_wifi_sta_set_credentials()`, lowers the auth threshold to `WIFI_AUTH_OPEN` if no password was given, and sets `auto_connect_on_start = true`.
- **Capabilities:**
  - Accepts either `"ssid password"` (secured AP) or `"ssid"` alone (open AP) from the serial console before starting or connecting the handle.

### 3.2 Driver ops callbacks (`g_net_iface_wifi_sta_ops`)

Called by `net_iface.c` in lifecycle order:

1. **`wifi_ops_start(self)`**
   - **What it does:** Calls `esp_wifi_init()`, creates the STA `esp_netif` (`esp_netif_create_wifi`), installs default STA handlers, sets RAM storage and STA mode, calls `esp_wifi_start()`, registers `on_wifi_sta_connected` and `on_wifi_sta_disconnected`, and calls `net_iface_notify_started()`.
   - **Capabilities:** Detects if `esp_wifi_init()` was already called by the application (`ESP_ERR_INVALID_STATE`) and sets `s_wifi_we_inited = false` so `wifi_ops_stop()` will not deinitialize a shared radio.
2. **`wifi_ops_connect(self)`**
   - **What it does:** Resets `retry_num = 0`, writes `h->cfg.wifi_sta` to the driver via `esp_wifi_set_config(WIFI_IF_STA, ...)`, and calls `esp_wifi_connect()`.
   - **Capabilities:** Returns `ESP_ERR_INVALID_STATE` without touching the radio if `ssid` is empty, allowing the handle to stay in `STARTED` until credentials are provided.
3. **`wifi_ops_disconnect(self)`**
   - **What it does:** Sets `s_wifi_ignore_disconnect = true` and calls `esp_wifi_disconnect()`.
   - **Capabilities:** Suppresses automatic reconnect on the resulting `WIFI_EVENT_STA_DISCONNECTED` event while leaving the Wi-Fi radio and `esp_netif` running.
4. **`wifi_ops_stop(self)`**
   - **What it does:** Sets `s_wifi_ignore_disconnect = true`, unregisters the STA event handlers, calls `esp_wifi_stop()`, calls `esp_wifi_deinit()` only if `s_wifi_we_inited` is true, clears default STA handlers, and destroys `s_wifi_netif`.
   - **Capabilities:** Tears down the STA netif and stops Wi-Fi cleanly without disturbing a radio initialized outside `net_connect`.
5. **`wifi_ops_deinit(self)`**
   - **What it does:** Clears the legacy `s_wifi_sta_handle` pointer if it points to `self`.
   - **Capabilities:** Keeps the legacy `net_configure_wifi_sta()` singleton pointer from becoming dangling when the handle is freed via `net_iface_deinit()`.

### 3.3 Legacy Wi-Fi APIs (`include/net_connect_wifi_config.h`)

1. **`net_configure_wifi_sta(config)`**
   - **Arguments:** `config` (`net_wifi_sta_config_t *`, or `NULL` for Kconfig defaults).
   - **What it does:** Creates or updates the singleton Wi-Fi STA handle (`s_wifi_sta_handle`) and returns it.
   - **Capabilities:** Can update settings on an existing Wi-Fi handle as long as it is in `INIT`, `STARTED`, or `FAILED`, and shares that handle with `net_connect()`.
2. **`net_connect_wifi_is_configured()`**
   - **Arguments:** None.
   - **What it does:** Returns `true` if `s_wifi_sta_handle` has been created via `net_configure_wifi_sta()`.
   - **Capabilities:** Lets callers check whether Wi-Fi STA has been configured before calling `net_connect_wifi()`.
3. **`net_connect_wifi()`**
   - **Arguments:** None.
   - **What it does:** Reads `stdin` credentials if enabled, starts and connects `s_wifi_sta_handle`, and blocks in `net_iface_wait()` for `CONFIG_NET_CONNECT_TIMEOUT_MS`.
   - **Capabilities:** All-in-one blocking connect for the configured Wi-Fi STA handle; automatically stops the handle if waiting fails or times out.
4. **`net_disconnect_wifi()`**
   - **Arguments:** None.
   - **What it does:** Calls `net_iface_stop()` and `net_iface_deinit()` on the Wi-Fi STA handle.
   - **Capabilities:** Stops the Wi-Fi driver, destroys its `esp_netif`, and frees the handle.

---

## 4. Ethernet layer (`eth_connect.c`)

Declared in `include/net_iface.h`.

### 4.1 Public creation API

#### `net_eth_create()`
```c
esp_err_t net_eth_create(const net_eth_params_t *params, net_iface_handle_t *out_handle);
```
- **Arguments:**
  - `params`: Optional Ethernet parameters (`route_prio`, `required`, `auto_connect_on_start`, `ip`), or `NULL` for Kconfig defaults.
  - `out_handle`: Output pointer receiving the new Ethernet handle.
- **What it does:** Fills `net_iface_config_t` with Ethernet defaults (`route_prio = 64`, `auto_connect_on_start = true`, DHCP on), applies `params`, and calls `net_iface_init_with_ops(&cfg, &g_net_iface_eth_ops, out_handle)`.
- **Capabilities:**
  - Creates the Ethernet handle in `INIT` state without touching the MAC, PHY, or SPI/RMII bus.
  - Allows overriding route priority, `required` flag, `auto_connect_on_start`, and IPv4/IPv6 settings per handle.
  - Returns `ESP_ERR_NOT_SUPPORTED` when Ethernet is disabled in Kconfig.

### 4.2 Driver ops callbacks (`g_net_iface_eth_ops`)

Called by `net_iface.c` in lifecycle order:

1. **`eth_ops_start(self)`**
   - **What it does:** Calls `ethernet_init_all(&s_eth_handles, &s_eth_count)`, creates `s_eth_netif` (`esp_netif_new`), creates and attaches the netif glue for `s_eth_handles[0]` (`esp_eth_new_netif_glue`, `esp_netif_attach`), registers `on_eth_event` for `ETH_EVENT`, and calls `net_iface_notify_started()`.
   - **Capabilities:** Initializes the Ethernet MAC/PHY hardware and creates the `esp_netif` without starting link autonegotiation yet.
2. **`eth_ops_connect(self)`**
   - **What it does:** Calls `esp_eth_start(s_eth_handles[0])`.
   - **Capabilities:** Starts the PHY state machine; when a cable is plugged in, `ETHERNET_EVENT_CONNECTED` transitions the handle to `CONNECTED` and starts DHCP.
3. **`eth_ops_disconnect(self)`**
   - **What it does:** Calls `esp_eth_stop(s_eth_handles[0])`.
   - **Capabilities:** Stops the PHY and drops the link while keeping the MAC, netif glue, and `s_eth_netif` allocated in `STARTED` for a fast reconnect.
4. **`eth_ops_stop(self)`**
   - **What it does:** Calls `eth_release_resources()` to unregister `on_eth_event`, stop the Ethernet driver, delete the netif glue, destroy `s_eth_netif`, and call `ethernet_deinit_all()`.
   - **Capabilities:** Releases all Ethernet hardware handles and netif memory so the interface can be cleanly restarted or deinitialized.

---

## 5. OpenThread layer (`thread_connect.c`)

Declared in `include/net_iface.h`.

### 5.1 Public creation API

#### `net_thread_create()`
```c
esp_err_t net_thread_create(const net_thread_params_t *params, net_iface_handle_t *out_handle);
```
- **Arguments:**
  - `params`: Optional Thread parameters (`task_stack_size`, `route_prio`, `required`, `auto_connect_on_start`, `ip`), or `NULL` for Kconfig defaults.
  - `out_handle`: Output pointer receiving the new OpenThread handle.
- **What it does:** Fills `net_iface_config_t` with Thread defaults (`route_prio = 16`, `auto_connect_on_start = true`, `enable_ipv6 = true`, `task_stack_size = CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE`), applies `params`, and calls `net_iface_init_with_ops(&cfg, &g_net_iface_thread_ops, out_handle)`.
- **Capabilities:**
  - Creates the OpenThread handle in `INIT` state without registering `eventfd`, touching the 802.15.4 radio, or spawning the OpenThread worker task.
  - Allows customizing the OpenThread worker task stack size, route priority, and `auto_connect_on_start`.
  - Returns `ESP_ERR_NOT_SUPPORTED` when OpenThread is disabled in Kconfig.

### 5.2 Driver ops callbacks (`g_net_iface_thread_ops`)

Called by `net_iface.c` in lifecycle order:

1. **`thread_ops_start(self)`**
   - **What it does:** Creates `s_ot_stop_sem`, registers `esp_vfs_eventfd_register()`, registers `on_thread_dns_server` for `OPENTHREAD_EVENT_SET_DNS_SERVER`, and spawns `ot_task_worker` (`"ot_br_main"`). Inside `ot_task_worker`, the worker creates `s_openthread_netif`, initializes the OpenThread stack and netif glue, calls `net_iface_notify_started()`, optionally triggers `net_iface_connect()`, and runs `esp_openthread_launch_mainloop()`.
   - **Capabilities:** Tracks whether `eventfd` was registered by this driver (`s_eventfd_owned`) so it will not unregister an `eventfd` owned by the application, and isolates the blocking OpenThread main loop in its own dedicated task.
2. **`thread_ops_connect(self)`**
   - **What it does:** Acquires the OpenThread lock, reads the active operational dataset TLVs (`otDatasetGetActiveTlvs`), and calls `esp_openthread_auto_start()`.
   - **Capabilities:** Uses the stored active dataset if one exists in NVS, or falls back to the default Kconfig dataset when none is stored; readiness is then signaled when `OPENTHREAD_EVENT_ATTACHED` arrives.
3. **`thread_ops_disconnect(self)`**
   - **What it does:** Acquires the OpenThread lock and calls `otThreadSetEnabled(instance, false)`.
   - **Capabilities:** Leaves the Thread mesh while keeping `ot_task_worker`, the OpenThread instance, and `s_openthread_netif` running in `STARTED`.
4. **`thread_ops_stop(self)`**
   - **What it does:** Unregisters `on_thread_dns_server`, sets `s_ot_stop_requested = true`, disables Thread and IPv6, calls `esp_openthread_mainloop_exit()`, and waits up to 5 s on `s_ot_stop_sem` for `ot_task_worker` to deinitialize the netif, glue, OpenThread stack, and `eventfd` on its own thread.
   - **Capabilities:** Shuts down the OpenThread worker task cooperatively without calling `vTaskDelete()` from outside, preventing leaked OpenThread locks or buffers.

---

## 6. PPP layer (`ppp_connect.c`)

Declared in `include/net_iface.h`.

### 6.1 Public creation API

#### `net_ppp_create()`
```c
esp_err_t net_ppp_create(const net_ppp_params_t *params, net_iface_handle_t *out_handle);
```
- **Arguments:**
  - `params`: Optional PPP parameters (`max_retry`, `task_stack_size`, `route_prio`, `required`, `auto_connect_on_start`, `ip`), or `NULL` for Kconfig defaults.
  - `out_handle`: Output pointer receiving the new PPP handle.
- **What it does:** Fills `net_iface_config_t` with PPP defaults (`route_prio = 32`, `auto_connect_on_start = true`, `max_retry = CONFIG_NET_CONNECT_PPP_CONN_MAX_RETRY`, `task_stack_size = CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE`), applies `params`, and calls `net_iface_init_with_ops(&cfg, &g_net_iface_ppp_ops, out_handle)`.
- **Capabilities:**
  - Creates the PPP handle in `INIT` state without installing the TinyUSB or UART driver.
  - Allows overriding `max_retry`, UART worker stack size, route priority, `required` flag, and `auto_connect_on_start`.
  - Returns `ESP_ERR_NOT_SUPPORTED` when PPP is disabled in Kconfig.

### 6.2 Driver ops callbacks (`g_net_iface_ppp_ops`)

Called by `net_iface.c` in lifecycle order:

1. **`ppp_ops_start(self)`**
   - **What it does:** In USB mode, installs TinyUSB and initializes CDC-ACM (`ppp_usb_install()`). Creates the PPP `s_netif` with `s_ppp_driver_cfg` (`.transmit = transmit`), registers `on_ppp_lost_ip` for `IP_EVENT_PPP_LOST_IP`, and calls `net_iface_notify_started()`.
   - **Capabilities:** Prepares the PPP `esp_netif` and retry listener (plus USB CDC enumeration in USB mode) before starting LCP/IPCP negotiation.
2. **`ppp_ops_connect(self)`**
   - **What it does:** Resets `retry_num = 0`. In USB mode, calls `esp_netif_action_start()` and `esp_netif_action_connected()`. In UART mode, spawns `ppp_task` (`"ppp connect"`), which installs `UART_NUM_1`, starts PPP actions on `s_netif`, and pumps UART frames into `esp_netif_receive()`. Calls `net_iface_notify_connected(self)`.
   - **Capabilities:** Starts PPP negotiation over either USB CDC or UART and transitions the handle to `CONNECTED` while waiting for `IP_EVENT_PPP_GOT_IP` (retrying up to `max_retry` times on `IP_EVENT_PPP_LOST_IP`).
3. **`ppp_ops_disconnect(self)`**
   - **What it does:** In UART mode, signals `ppp_task` to exit and waits on `s_ppp_exit_sem` (`ppp_stop_uart_task()`). Calls `esp_netif_action_disconnected()` and `esp_netif_action_stop()`.
   - **Capabilities:** Stops PPP negotiation (and deletes the UART driver/task in UART mode) while keeping `s_netif` (and TinyUSB in USB mode) allocated in `STARTED`.
4. **`ppp_ops_stop(self)`**
   - **What it does:** Unregisters `on_ppp_lost_ip`, stops `ppp_task` if in UART mode, clears `s_netif`, stops and destroys the PPP `esp_netif` (after a 100 ms drain delay), and uninstalls TinyUSB if in USB mode.
   - **Capabilities:** Safely drains any in-flight USB/PPP callbacks before freeing the `esp_netif` and releasing the USB or UART transport.
