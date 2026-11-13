# Design decisions and justifications

Decisions in the current `components/net_connect` implementation and the engineering rationale behind each one.

---

## 1. Public API and handle model

1. **Opaque handle (`net_iface_handle_t`) plus a polymorphic ops table (`net_iface_ops_t`), instead of exposing `struct net_iface` or a separate state machine per driver.**
   - **Justification:** Hiding `struct net_iface` in `net_iface_priv.h` keeps lock ordering, sticky FreeRTOS event-group bits, startup-task handles, and state transitions private so callers cannot mutate them without taking the proper mutexes. Sharing one core state machine and wait engine across Wi-Fi, Ethernet, OpenThread, and PPP eliminates duplicated semaphores and IP event handlers in each driver, while `net_iface_ops_t` isolates hardware-specific calls to small driver files.

2. **Two-tier creation API: unified `net_iface_init(const net_iface_config_t *, ...)` and typed wrappers (`net_wifi_sta_create`, `net_eth_create`, `net_thread_create`, `net_ppp_create`) that accept `NULL` for Kconfig defaults.**
   - **Justification:** Most example and application callers only want Kconfig defaults or need to override one or two fields (such as SSID, `required`, or `route_prio`), which the typed `net_*_create()` helpers make concise. Full programmatic callers and generic table-driven code can still construct a complete `net_iface_config_t` and pass it to `net_iface_init()` without branching on interface type.

3. **Negative sentinels (`-1` / `<= 0`) in `net_*_params_t` (`max_retry`, `route_prio`, `task_stack_size`) so a zero-initialized struct is distinct from "use the Kconfig or built-in default".**
   - **Justification:** For `max_retry` and `route_prio`, `0` is a meaningful value (`max_retry = 0` means "try once and do not retry on disconnect"; `route_prio = 0` is a valid priority). Using `< 0` as "use Kconfig/default" allows `max_retry = 0` to be explicitly requested while keeping `<= 0` for `task_stack_size` where zero bytes is never a valid stack size.

4. **`net_ip_config_t ip` stored both inside `net_wifi_sta_config_t` and at the top of `net_iface_config_t`, and kept in sync on init and create.**
   - **Justification:** Legacy callers using `net_configure_wifi_sta(net_wifi_sta_config_t *)` pass IP settings nested inside `net_wifi_sta_config_t`, whereas the generic `net_iface.c` core reads `h->cfg.ip` uniformly for all interface types. Keeping both fields and synchronizing them in `net_iface_init()` and `net_wifi_sta_create()` preserves source compatibility with `net_wifi_sta_config_t` while avoiding type-specific IP lookups inside `net_iface.c`.

5. **Zero-argument `net_connect()` / `net_disconnect()` and the legacy Wi-Fi entry points (`net_configure_wifi_sta`, `net_connect_wifi`, `net_disconnect_wifi`) kept on top of the handle core.**
   - **Justification:** Existing ESP-IDF protocol examples call `net_connect()` / `net_disconnect()` or the `0.1.x` Wi-Fi helpers directly. Implementing them as thin wrappers over `net_iface_*` avoids breaking existing applications and avoids maintaining two parallel connection implementations.

6. **Disabled `net_*_create()` functions return `ESP_ERR_NOT_SUPPORTED` instead of hiding the declarations behind `#if CONFIG_NET_CONNECT_*`.**
   - **Justification:** Keeping all four `net_*_create()` declarations unconditionally visible lets multi-interface application code compile without `#ifdef` clutter around every creation call and enables unit tests to verify the disabled-interface error path at runtime.

7. **`net_connect()` is a no-op inline that returns `ESP_OK` when `CONFIG_IDF_TARGET_LINUX` is set.**
   - **Justification:** On the Linux host target, the OS already provides network connectivity and ESP-IDF's hardware drivers (`esp_wifi`, `esp_eth`, `esp_openthread`) are not used. Returning `ESP_OK` from a header inline lets protocol examples compile and run unchanged on the Linux target.

---

## 2. State machine and lifecycle

8. **Eight-state lifecycle (`UNINIT`, `INIT`, `STARTING`, `STARTED`, `CONNECTING`, `CONNECTED`, `READY`, `STOPPING`, `FAILED`). `STARTED` is separate from `CONNECTING`. `CONNECTED` (L2 up) is separate from `READY` (address or mesh attach).**
   - **Justification:** Each state represents an observable boundary that applications and tests need to distinguish:
     - `STARTING` vs. `STARTED`: `net_iface_start()` is non-blocking, so the handle is in `STARTING` while the background task initializes the driver, and enters `STARTED` once `h->netif` exists and the radio/MAC is up.
     - `STARTED` vs. `CONNECTING`: A station with no SSID yet (or an interface after `net_iface_disconnect()`) has its driver and `esp_netif` running (`STARTED`) without actively joining a network (`CONNECTING`).
     - `CONNECTED` vs. `READY`: L2 association or cable link-up (`CONNECTED`) happens before DHCP / SLAAC finishes; sockets cannot bind or send traffic until required L3 addresses are assigned (`READY`).

9. **Create does not touch hardware. `net_iface_init_with_ops()` only allocates the handle, locks, and event group in `INIT`. Driver, radio, and PHY work starts in `net_iface_start()`.**
   - **Justification:** Separating object allocation/configuration from hardware bring-up lets callers create and inspect handles, register event listeners, or set credentials before any radio/PHY interrupts or events can fire, and keeps creation fast and deterministic in unit tests.

10. **`auto_connect_on_start` couples start and connect by default, and lets a caller (or an empty Wi-Fi SSID) stop in `STARTED` for later provisioning.**
    - **Justification:** In the common case, starting an interface should immediately join the network without requiring a second API call. When credentials are provided later (via BLE/SoftAP provisioning, console command, or `stdin`), setting `auto_connect_on_start = false` brings the radio and `esp_netif` up in `STARTED` without triggering spurious failed connection attempts.

11. **`CONNECTING` goes back to `STARTED`, not `FAILED`, when `ops->connect()` returns `ESP_ERR_INVALID_STATE` (for example Wi-Fi connect before an SSID is set).**
    - **Justification:** Calling `net_iface_connect()` when credentials are not yet configured is a precondition error by the caller, not a hardware or network failure. Reverting to `STARTED` leaves the driver healthy and prevents `net_iface_wait()` from seeing a sticky `FAILED` bit once credentials are set.

12. **`net_iface_disconnect()` returns to `STARTED` and keeps the driver and netif. `net_iface_stop()` returns to `INIT` and destroys them.**
    - **Justification:** Dropping the L2 link (for example to switch Wi-Fi APs in the console command or to pause traffic) should not pay the cost of tearing down and recreating `esp_netif`, the Wi-Fi driver, or the OpenThread worker task. `net_iface_stop()` is reserved for full driver shutdown when the `esp_netif` and hardware resources must be freed.

13. **`net_iface_deinit()` is allowed only from `INIT`. A running handle returns `ESP_ERR_INVALID_STATE` instead of being stopped implicitly.**
    - **Justification:** `net_iface_stop()` can block waiting for background tasks (`iface_startup_task`, `ot_task_worker`, `ppp_task`) and cannot be called from the event loop. Making `net_iface_stop()` an explicit prerequisite before `net_iface_deinit()` keeps lifecycle ownership clear and prevents unexpected blocking or use-after-free while driver callbacks are still registered.

14. **`net_iface_stop()` always ends in `net_iface_notify_stopped()` (`INIT`) even when `ops->stop()` returns an error, so a handle cannot stay in `STOPPING`.**
    - **Justification:** If a driver's `ops->stop()` reports an error, leaving the handle in `STOPPING` would wedge it permanently: `net_iface_stop()` would return early on a retry, and `net_iface_deinit()` would reject it because the state is not `INIT`. Forcing the transition to `INIT` (while still returning the driver error code to the caller) guarantees the handle can always be freed or restarted.

15. **`net_iface_notify_failed()` rewrites `ESP_OK` to `ESP_FAIL`, so a failed interface never stores `ESP_OK` as `last_err`.**
    - **Justification:** `net_iface_wait()` returns `h->last_err` when `NET_IFACE_BIT_FAILED` is set. If a buggy caller or driver passed `ESP_OK` into `net_iface_notify_failed()`, `net_iface_wait()` would return `ESP_OK` on a failed interface and mislead the application into thinking the interface is ready.

---

## 3. Non-blocking startup

16. **`net_iface_start()` creates a short-lived per-interface task (`iface_startup_task`, priority 5) and returns immediately, so slow init (Ethernet PHY, Wi-Fi RF calibration) can run in parallel.**
    - **Justification:** Hardware initialization can block for hundreds of milliseconds to several seconds (for example, Ethernet PHY reset/autonegotiation or Wi-Fi NVS and RF init). Running `ops->start()` in a dedicated transient task makes `net_iface_start()` non-blocking and allows multiple interfaces (such as Ethernet + Wi-Fi + Thread in a home hub) to initialize concurrently.

17. **`xTaskCreate(iface_startup_task, ..., &h->startup_task)` runs while `h->lock` is held, so the new task cannot clear `h->startup_task` before the handle is stored.**
    - **Justification:** In FreeRTOS, if the newly created task has a higher or equal priority, the scheduler can switch to `iface_startup_task` before `xTaskCreate()` writes the output `pxCreatedTask` pointer (`&h->startup_task`). Holding `h->lock` across `xTaskCreate()` forces the cleanup block in `iface_startup_task` (which takes `h->lock` before setting `h->startup_task = NULL`) to wait until `net_iface_start()` has finished storing `h->startup_task`.

18. **`NET_IFACE_BIT_STARTUP_DONE` is not part of `NET_IFACE_BITS_LIVENESS`, so a later disconnect does not clear the "startup task has exited" signal.**
    - **Justification:** `NET_IFACE_BITS_LIVENESS` (`READY | FAILED | STOPPED | IPV4 | IPV6 | OT_ATTACHED`) is cleared whenever an interface disconnects or stops. If `STARTUP_DONE` were included in that mask, a disconnect or stop happening while another task (such as `console_wait_until_started()` or `net_iface_stop()`) is waiting on `STARTUP_DONE` could clear the bit and cause a 10-second timeout even though the startup task already finished.

19. **Stop during startup is cooperative: set `cancel_requested`, wait up to 10 s for `NET_IFACE_BIT_STARTUP_DONE`. The startup task is not deleted from outside.**
    - **Justification:** Calling `vTaskDelete()` from another task while `iface_startup_task` is inside `esp_wifi_init()`, `ethernet_init_all()`, or `esp_event_handler_register()` would leak heap memory and leave ESP-IDF internal mutexes permanently locked. Waiting for `iface_startup_task` to finish `ops->start()`, observe `cancel_requested`, and exit cleanly keeps ESP-IDF drivers in a consistent state.

20. **`net_iface_notify_started()` stores `h->netif` even when the state is already `STOPPING`, and does not enter `STARTED`, so a racing stop still knows which netif to free.**
    - **Justification:** When `net_iface_stop()` is called while `ops->start()` is already running in `iface_startup_task`, `h->state` becomes `STOPPING` before `ops->start()` calls `net_iface_notify_started(h, netif)`. Saving `h->netif` without transitioning to `STARTED` prevents the stopping interface from appearing started while still recording the `esp_netif_t *` created by `ops->start()`.

21. **`net_iface_stop()` always calls `ops->disconnect()` before `ops->stop()`, including when stop arrived during `STARTING` and the link came up before the startup task exited.**
    - **Justification:** By the time `net_iface_stop()` finishes waiting for `iface_startup_task`, the driver may have already started or begun associating. Calling `ops->disconnect()` unconditionally before `ops->stop()` ensures active links or background worker tasks (such as `ppp_task`) are cleanly dropped before the underlying `esp_netif` is destroyed.

22. **`iface_startup_task` records `uxTaskGetStackHighWaterMark()` in `h->startup_stack_hwm` before it deletes itself.**
    - **Justification:** Because `iface_startup_task` deletes itself (`vTaskDelete(NULL)`) as soon as startup completes, an external caller cannot inspect its stack usage afterward. Saving the high-water mark in `h->startup_stack_hwm` allows unit tests (`test_net_connect.c`) to assert that `CONFIG_NET_CONNECT_STARTUP_TASK_STACK_SIZE` has safe headroom without stack overflow risk.

---

## 4. Readiness and wait APIs

23. **Readiness is tracked in `net_iface.c` by one shared `IP_EVENT` listener (`on_global_ip_event`) and one `OPENTHREAD_EVENT` listener (`on_global_ot_event`), not by per-driver semaphores or IP handlers.**
    - **Justification:** In ESP-IDF, `IP_EVENT_GOT_IP6` is shared across Wi-Fi, Ethernet, and PPP and identifies the interface via `event->esp_netif`. Matching `event->esp_netif` against the registry in one central handler (`find_iface_by_netif()`) avoids every driver registering duplicate `IP_EVENT` callbacks and ensures identical IPv4/IPv6 readiness rules across all IP-based drivers.

24. **`is_ready_satisfied_locked()` is type-specific. Thread is ready on `NET_IFACE_BIT_OT_ATTACHED` only. Wi-Fi, Ethernet, and PPP need `NET_IFACE_BIT_IPV4` when IPv4 is enabled, and `NET_IFACE_BIT_IPV6` of `NET_CONNECT_PREFERRED_IPV6_TYPE` when IPv6 is enabled on the handle.**
    - **Justification:** OpenThread manages its own mesh-local and RLOC IPv6 addresses internally and signals operational readiness via `OPENTHREAD_EVENT_ATTACHED` rather than lwIP `IP_EVENT_GOT_IP6`. Conversely, Wi-Fi, Ethernet, and PPP rely on lwIP DHCP / SLAAC / IPCP and are only usable by application sockets once all configured address families (IPv4 and/or the preferred IPv6 scope) have been assigned.

25. **`net_iface_notify_connected()` calls `esp_netif_create_ip6_linklocal()` for every non-Thread interface when IPv6 is enabled.**
    - **Justification:** In ESP-IDF lwIP, IPv6 SLAAC and link-local address assignment do not start on L2 link-up unless `esp_netif_create_ip6_linklocal()` is triggered after the link is up. Doing this once in `net_iface_notify_connected()` (for all non-Thread interfaces) avoids repeating IPv6 bring-up code in `wifi_connect.c`, `eth_connect.c`, and `ppp_connect.c`.

26. **`net_iface_wait()` uses sticky event-group bits (`xClearOnExit = pdFALSE`, `xWaitForAllBits = pdFALSE`), so a late waiter returns immediately and more than one task can wait on the same handle.**
    - **Justification:** With a binary semaphore or auto-clearing event bits, if an interface reaches `READY` before `net_iface_wait()` is called—or if two application tasks both call `net_iface_wait()` on the same handle—one of the tasks would block forever. Keeping `READY`, `FAILED`, and `STOPPED` sticky until the next state transition makes `net_iface_wait()` level-triggered and race-free.

27. **`net_iface_wait()` checks `READY` before `FAILED`, and `FAILED` before `STOPPED`.**
    - **Justification:** Deterministic priority ordering ensures that if `READY` is set, the caller always receives `ESP_OK`, and if an interface failed vs. was stopped, the specific failure error (`h->last_err`) is reported rather than a generic `ESP_ERR_INVALID_STATE`.

28. **`ESP_ERR_TIMEOUT` from `net_iface_wait*()` leaves the connection attempt running.**
    - **Justification:** A wait timeout only means the caller's wait budget expired; the station or PHY may be partway through association or DHCP. Leaving the interface running allows the caller to either wait again, continue running in degraded/offline mode while `NET_IFACE_EVENT_READY` arrives later in the background, or explicitly call `net_iface_stop()` if it wants to abort.

29. **`wait_all` / `wait_any` use per-handle sticky bits plus one shared pulse (`NET_IFACE_GLOBAL_WAKE_BIT`). The pulse is cleared before each scan so a notify during the scan is not lost.**
    - **Justification:** FreeRTOS `xEventGroupWaitBits()` cannot block on multiple event groups simultaneously. Using a single shared event group (`s_global_wait_events`) purely as a "re-scan now" pulse—and clearing `NET_IFACE_GLOBAL_WAKE_BIT` *before* inspecting each handle's sticky `state_events`—eliminates the classic check-then-sleep lost-wakeup race without allocating per-waiter semaphores or linked lists.

30. **`wait_all()` fails on the first `FAILED` or `STOPPED` handle. `wait_any()` keeps waiting while any handle is still trying, and fails only when every handle has failed or stopped.**
    - **Justification:** `wait_all()` requires conjunction (every handle must reach `READY`), so as soon as one handle reaches a terminal non-ready state (`FAILED` or `STOPPED`), the conjunction can never succeed and failing fast avoids wasting the remaining timeout. `wait_any()` requires disjunction (at least one handle must reach `READY`), so one interface failing (e.g., unplugged Ethernet) must not abort the wait while another interface (e.g., Wi-Fi) is still connecting.

31. **`net_iface_notify_lost_ip()` does not pulse `s_global_wait_events`. It moves `READY` back to `CONNECTING`.**
    - **Justification:** Losing an IP lease (`IP_EVENT_*_LOST_IP`) or unplugging an Ethernet cable puts the interface back into `CONNECTING` (a non-terminal, in-progress state), not `READY`, `FAILED`, or `STOPPED`. Pulsing `s_global_wait_events` would only wake `wait_all` / `wait_any` tasks to re-scan and go right back to sleep.

---

## 5. Concurrency and event-loop safety

32. **Two locks, always in the same order: `s_registry_mutex`, then `h->lock`.**
    - **Justification:** Registry lookups (`find_iface_by_netif`, `net_iface_get_by_type`, `net_iface_deinit`) must hold `s_registry_mutex` to walk `s_iface_list` and also inspect `h->netif` or `h->state` under `h->lock`. Enforcing `s_registry_mutex` → `h->lock` everywhere makes an AB-BA deadlock between two tasks impossible.

33. **Neither lock is held across `ops->*`, `esp_event_handler_register` / `unregister`, or `esp_event_post`.**
    - **Justification:** Driver callbacks (`ops->*`) and `esp_event_*` functions acquire internal ESP-IDF locks (Wi-Fi lock, event-loop mutex, OpenThread lock) and may synchronously invoke callbacks that re-enter `net_iface_*` (which takes `s_registry_mutex` and `h->lock`). Releasing all `net_iface` locks before calling external layers prevents lock-order inversions and re-entrancy deadlocks.

34. **`post_iface_event()` posts with a timeout of 0, so a full event-loop queue does not stall a state change.**
    - **Justification:** `post_iface_event()` is often called from the default event loop task itself (inside `on_global_ip_event` or `on_wifi_sta_connected`). If the default event loop queue were full and `esp_event_post()` blocked with `portMAX_DELAY`, the event loop task would block waiting for itself to drain the queue, deadlocking the system. Meanwhile, `net_iface_wait*()` relies on sticky event-group bits rather than `NET_IFACE_EVENT`, so dropping an event under extreme queue saturation never breaks `net_iface_wait*()`.

35. **`net_iface_register_event_handler()` does not replay past events. The caller reads `net_iface_get_state()` after registering.**
    - **Justification:** Synthesizing fake past events on registration would either run callbacks synchronously on the caller's stack (violating the contract that callbacks run on the default event loop) or post duplicate events to *all* registered listeners on the `NET_IFACE_EVENT` base. Querying `net_iface_get_state()` after registration is simple, side-effect-free, and race-free.

36. **`s_event_loop_task` is recorded inside `on_global_iface_event`, `on_global_ip_event`, and `on_global_ot_event`. `NET_IFACE_EVENT` is registered first so that task is known before an application callback runs.**
    - **Justification:** ESP-IDF's `esp_event` API does not expose a public getter for the default event loop's FreeRTOS `TaskHandle_t`. Recording `xTaskGetCurrentTaskHandle()` inside the first handler registered on `NET_IFACE_EVENT` (and on `IP_EVENT` / `OPENTHREAD_EVENT`) guarantees `s_event_loop_task` is populated before any user `NET_IFACE_EVENT` handler executes on that task.

37. **`net_iface_wait()`, `wait_all()`, `wait_any()`, and `net_iface_stop()` return `ESP_ERR_INVALID_STATE` when called from `s_event_loop_task`.**
    - **Justification:** Readiness events (`IP_EVENT_*_GOT_IP`, `WIFI_EVENT_STA_CONNECTED`) and driver shutdown callbacks are dispatched on the default event loop task. If an application callback running on that task blocked inside `net_iface_wait*()` or `net_iface_stop()`, the event loop task would freeze and could never process the event required to complete the wait or stop.

38. **`s_registry_mutex`, `s_global_wait_events`, and the shared event handlers are created on the first handle and destroyed when the last handle is freed, so a unit-test leak check can return to the baseline heap.**
    - **Justification:** Unity unit tests in ESP-IDF record free heap before a test case and assert zero bytes leaked after the test case finishes, and tests also delete and recreate the default event loop between cases. Lazily allocating global primitives on the first `init` and freeing/unregistering them when the last handle is `deinit`ed ensures zero residual heap allocation and no dangling handlers on a deleted event loop.

39. **`try_register_global_handlers()` runs from both `net_iface_init_with_ops()` and `net_iface_start()`, so a handle can be created before the default event loop exists.**
    - **Justification:** Some applications create or configure interface handles before calling `esp_event_loop_create_default()`, while others create the event loop first. Trying registration during `init` and retrying idempotently during `start` supports both initialization orders without requiring a strict call sequence.

40. **`net_iface_for_each()` copies up to `NET_IFACE_WALK_MAX` (16) handles under the list lock and runs callbacks after releasing it. New handles are prepended; `connect.c` walks the snapshot backwards to keep create order.**
    - **Justification:** Holding `s_registry_mutex` while invoking a user callback (`cb`) would deadlock or block if `cb` calls `net_iface_get_state()`, `net_iface_stop()`, or `net_iface_get_by_type()`. Taking a bounded stack snapshot under the lock is \(O(N)\) with no heap allocation; prepending to `s_iface_list` keeps insertion \(O(1)\), and walking backwards in `connect.c` restores FIFO creation order when starting and waiting on interfaces.

---

## 6. Per-driver choices

41. **Default route priority: Wi-Fi 128, Ethernet 64, PPP 32, Thread 16, set on `esp_netif_inherent_config_t.route_prio`.**
    - **Justification:** Giving each interface type a distinct default `route_prio` ensures deterministic default-route selection in lwIP when multiple interfaces are active simultaneously, while allowing applications to override `route_prio` per handle (for example, setting Ethernet higher than Wi-Fi on a wired-primary gateway).

42. **Wi-Fi (`wifi_connect.c`):**
    - **`s_wifi_we_inited` stays false when `esp_wifi_init()` returns `ESP_ERR_INVALID_STATE`, so `wifi_ops_stop()` does not call `esp_wifi_deinit()` on a radio this driver did not init.**
      - **Justification:** An application (or coexisting SoftAP / BLE provisioning service) may have already called `esp_wifi_init()` before starting `net_connect`. Tearing down the radio in `wifi_ops_stop()` when `s_wifi_we_inited` is false would break the application's own Wi-Fi state.
    - **Station config is stored in RAM (`WIFI_STORAGE_RAM`).**
      - **Justification:** `net_connect` manages credentials in `h->cfg.wifi_sta` (from Kconfig, parameters, `stdin`, or console). Using `WIFI_STORAGE_RAM` avoids unnecessary flash wear on every connect and prevents stale credentials left in NVS from overriding the current run's configuration.
    - **`s_wifi_ignore_disconnect` suppresses reconnect on an intentional disconnect or stop. `WIFI_REASON_ROAMING` is ignored.**
      - **Justification:** Calling `esp_wifi_disconnect()` or `esp_wifi_stop()` asynchronously triggers `WIFI_EVENT_STA_DISCONNECTED`. Without `s_wifi_ignore_disconnect`, `on_wifi_sta_disconnected()` would treat an intentional disconnect as a link drop and immediately call `esp_wifi_connect()` again. Similarly, `WIFI_REASON_ROAMING` indicates that 802.11 roaming is already switching APs internally and must not increment `retry_num` or trigger a duplicate `esp_wifi_connect()`.
    - **`net_wifi_sta_set_credentials()` copies the SSID and password into `h->cfg.wifi_sta` only. `esp_wifi_set_config()` runs in `wifi_ops_connect()`.**
      - **Justification:** `net_wifi_sta_set_credentials()` is allowed in `INIT` (before `esp_wifi_init()` and `esp_wifi_set_mode(WIFI_MODE_STA)` have run), where calling `esp_wifi_set_config()` would fail with `ESP_ERR_WIFI_NOT_INIT`. Deferring `esp_wifi_set_config()` to `wifi_ops_connect()` works identically whether credentials were set in `INIT`, `STARTED`, or `FAILED`.

43. **Ethernet (`eth_connect.c`):**
    - **`eth_ops_start()` creates the MAC, netif, and glue. `esp_eth_start()` is `eth_ops_connect()`. Only `s_eth_handles[0]` from `ethernet_init_all()` is attached.**
      - **Justification:** Splitting `ethernet_init_all()` / `esp_netif_new()` (`eth_ops_start`) from `esp_eth_start()` (`eth_ops_connect`) matches the `STARTED` vs. `CONNECTING` lifecycle contract: in `STARTED`, `h->netif` exists and can be configured (e.g., static IP or route priority), and `eth_ops_disconnect()` / `eth_ops_connect()` can toggle the PHY with `esp_eth_stop()` / `esp_eth_start()` without destroying the MAC or `esp_netif`. Attaching `s_eth_handles[0]` matches the single-netif-per-handle model of `struct net_iface`.
    - **`ETHERNET_EVENT_DISCONNECTED` calls `net_iface_notify_lost_ip()` (`CONNECTING`), not disconnect or failed, so plugging the cable back in recovers without a retry limit.**
      - **Justification:** Unlike Wi-Fi association or PPP dial-up, an unplugged Ethernet cable requires no software retry loop—the PHY hardware continuously polls link status and automatically posts `ETHERNET_EVENT_CONNECTED` whenever the cable is plugged back in. Moving to `CONNECTING` via `net_iface_notify_lost_ip()` clears `READY` while keeping the interface waiting for the cable to be reconnected rather than permanently failing.

44. **OpenThread (`thread_connect.c`):**
    - **The stack runs on `ot_task_worker` (`"ot_br_main"`), not on `iface_startup_task`.**
      - **Justification:** `esp_openthread_launch_mainloop()` is a blocking loop that runs for the entire lifetime of the Thread interface and requires a large stack (`CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE`, default 8192 bytes), whereas `iface_startup_task` is a short-lived task (default 4096 bytes) that must exit and set `NET_IFACE_BIT_STARTUP_DONE` so `net_iface_stop()` can synchronize with startup completion.
    - **`s_eventfd_owned` is set only when this driver registered eventfd, so stop unregisters only what it registered.**
      - **Justification:** `esp_vfs_eventfd_register()` is a process-wide VFS registration that may already be used by other components in the application. Recording `s_eventfd_owned` when `esp_vfs_eventfd_register()` returns `ESP_OK` (and leaving it `false` on `ESP_ERR_INVALID_STATE`) prevents `thread_ops_stop()` from breaking another component's `eventfd` descriptors.
    - **Auto-connect runs only when `net_iface_get_state()` is `STARTED`, so the startup task and the worker do not both call connect.**
      - **Justification:** Because `thread_ops_start()` spawns `ot_task_worker` and returns immediately, `iface_startup_task` and `ot_task_worker` run concurrently. If `ot_task_worker` reaches `net_iface_notify_started()` before `iface_startup_task` checks `h->state`, either task could trigger auto-connect; checking `net_iface_get_state(self) == NET_IFACE_STATE_STARTED` ensures `net_iface_connect()` runs exactly once.
    - **Stop is cooperative: `s_ot_stop_requested`, `esp_openthread_mainloop_exit()`, `s_ot_task_published`, and a wait of up to 5 s on `s_ot_stop_sem`. The worker frees the netif, the OpenThread stack, and eventfd on its own task.**
      - **Justification:** OpenThread's stack and netif glue are not safe to deinitialize from another thread while `esp_openthread_launch_mainloop()` is active, nor can `ot_task_worker` be killed with `vTaskDelete()` from outside without leaking OpenThread buffers and locks. Signaling `esp_openthread_mainloop_exit()` (and `s_ot_stop_requested` if the worker has not entered the loop yet) lets `ot_task_worker` tear down the netif, glue, OpenThread instance, and `eventfd` in reverse initialization order on its own thread before signaling `s_ot_stop_sem`.

45. **PPP (`ppp_connect.c`):**
    - **`ppp_ops_connect()` calls `net_iface_notify_connected()` as soon as negotiation starts. There is no separate L2-up event.**
      - **Justification:** ESP-IDF's PPP netif glue does not emit a separate L2 "physical link connected" event before LCP/IPCP negotiation; it only emits `IP_EVENT_PPP_GOT_IP` and `IP_EVENT_PPP_LOST_IP`. Advancing to `CONNECTED` inside `ppp_ops_connect()` keeps the state machine consistent (`STARTED` → `CONNECTING` → `CONNECTED` → `READY`) so `IP_EVENT_PPP_GOT_IP` transitions from `CONNECTED` to `READY` just like Wi-Fi and Ethernet.
    - **USB installs TinyUSB in `ppp_ops_start()`. UART creates `ppp_task` in `ppp_ops_connect()` and deletes the UART driver on disconnect or stop.**
      - **Justification:** In USB CDC mode, TinyUSB device enumeration is driver-level transport setup (`start`) and incoming bytes arrive via TinyUSB's interrupt/task callback (`cdc_rx_callback`). In UART mode, reading UART frames requires a dedicated polling/queue task (`ppp_task`) and owns `UART_NUM_1` plus `esp_netif_action_start/connected`, so tying `ppp_task` to `connect()` / `disconnect()` stops the RX pump whenever the PPP link is disconnected.
    - **`uart_set_rx_timeout(UART_NUM_1, 1)` frames UART bytes before `esp_netif_receive()`.**
      - **Justification:** Setting a 1-symbol UART RX timeout causes the UART driver to post a `UART_DATA` event with `event.timeout_flag = true` as soon as an incoming burst of bytes pauses, allowing `ppp_task` to read complete PPP chunks from the ring buffer instead of waking up on every single byte.
    - **`IP_EVENT_PPP_LOST_IP` (`on_ppp_lost_ip`) retries until `max_retry`, then calls `net_iface_notify_failed()`.**
      - **Justification:** When `pppd` on the host is not ready or drops the session, lwIP reports `IP_EVENT_PPP_LOST_IP`. Retrying `esp_netif_action_start()` + `esp_netif_action_connected()` up to `max_retry` tolerates transient serial/host delays while bounding retries so a dead peer eventually transitions the handle to `FAILED`.
    - **`ppp_ops_stop()` clears `s_netif` before `esp_netif_destroy()` and delays 100 ms, so a late USB callback does not use a freed netif.**
      - **Justification:** `cdc_rx_callback()` runs asynchronously on the TinyUSB task and checks `if (s_netif == NULL) return;`. Nulling `s_netif` first, stopping PPP actions, and waiting 100 ms before calling `esp_netif_destroy(netif)` drains any in-flight `cdc_rx_callback()` or lwIP PPP timer callbacks before the `esp_netif` memory is freed.

---

## 7. Orchestrator, console, and console I/O

46. **`net_connect()` reuses a handle from `net_iface_get_by_type()` before creating a default one, so a caller can preconfigure a handle (or call `net_configure_wifi_sta()`) first.**
    - **Justification:** This bridges the low-level handle API and the high-level `net_connect()` helper: an application can create a handle with custom parameters (or call legacy `net_configure_wifi_sta()`) before calling `net_connect()`, and `net_connect()` will start and wait on that existing handle rather than failing or creating a duplicate interface.

47. **Required interfaces use `net_iface_wait_all()`. When none are required, connecting optional interfaces use `net_iface_wait_any()`. An optional failure does not fail `net_connect()`.**
    - **Justification:** In multi-interface devices (e.g., Ethernet required + Wi-Fi optional, or both optional with "whichever connects first"), `net_connect()` must guarantee that all mandatory interfaces are `READY` (`wait_all`), while still waiting for at least one usable network when every interface is marked optional (`wait_any`), and without tearing down working interfaces if an optional backup interface fails or is unprovisioned.

48. **`CONFIG_NET_CONNECT_WIFI_REQUIRED` defaults to `n` when `CONFIG_NET_CONNECT_ETHERNET` is enabled.**
    - **Justification:** When both Ethernet and Wi-Fi are enabled in `menuconfig`, Ethernet is typically the primary wired connection (for example in CI or on an Ethernet + Wi-Fi board where Wi-Fi credentials may not be set yet). Defaulting `WIFI_REQUIRED` to `n` in that combination lets `net_connect()` succeed as soon as Ethernet is ready without failing if the default placeholder Wi-Fi SSID (`"myssid"`) is not present.

49. **`net_connect()` registers `esp_register_shutdown_handler(net_disconnect_shutdown)`. On failure, `cleanup_all()` stops and frees every interface.**
    - **Justification:** Registering `net_disconnect_shutdown` ensures `esp_restart()` cleanly stops active radios/PHYs and sends clean disconnect frames before reset. Calling `cleanup_all()` on any `net_connect()` failure ensures `net_connect()` is atomic from the caller's perspective: either it returns `ESP_OK` with the required interfaces up, or it returns `ESP_FAIL` with all resources cleaned up.

50. **Address printing runs inside `esp_netif_tcpip_exec()`, so `esp_netif_next_unsafe()` runs on the TCP/IP task.**
    - **Justification:** `esp_netif_next_unsafe()` iterates the global `esp_netif` list without taking an internal lock and is documented by ESP-IDF as safe only inside `esp_netif_tcpip_exec()`. Dispatching `print_all_ips_tcpip()` to the TCP/IP task prevents data races if another task creates or destroys an `esp_netif` concurrently.

51. **The console creates an idle station (`ssid` empty, `auto_connect_on_start` false), waits on `NET_IFACE_BIT_STARTUP_DONE` before registering commands, and applies a non-zero channel with `esp_wifi_set_channel()` after `net_iface_connect()`.**
    - **Justification:** Interactive console users expect `wifi_connect <ssid> <pass>` to work even when `net_connect()` was never called; creating an idle station and waiting for `STARTUP_DONE` guarantees the radio is initialized before the first console command runs. Applying `esp_wifi_set_channel()` after `net_iface_connect()` keeps channel hints working for the console command without polluting the general `net_wifi_sta_config_t` struct.
