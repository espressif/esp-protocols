# Core lifecycle, walked through with OpenThread

This is the end-to-end path for **Flows 1–7 and Flow 10** in [IMPLEMENTATION_FLOWS.md](./IMPLEMENTATION_FLOWS.md), using OpenThread (`thread_connect.c`) as the concrete driver. The same core (`net_iface.c`) is shared by Wi-Fi STA, Ethernet, and PPP. OpenThread differs in two important ways:

1. It spawns a long-lived worker task (`ot_task_worker` / `"ot_br_main"`) that owns the OpenThread stack, netif glue, and `esp_openthread_launch_mainloop()`.
2. Readiness is satisfied by `OPENTHREAD_EVENT_ATTACHED` (`NET_IFACE_BIT_OT_ATTACHED`) rather than `IP_EVENT_*_GOT_IP`.

Sources:

- `include/net_iface_priv.h` — handle, ops table, sticky event-group bits
- `net_iface.c` — registry, state machine, waits, notifications, `on_global_ot_event`
- `thread_connect.c` — `g_net_iface_thread_ops`, `ot_task_worker`, and the ESP-IDF OpenThread calls

Line numbers below match those files as of this write-up.

---

## Call sequence

The application calls `net_thread_create()` to create the Thread handle. Start, connect, wait, disconnect, stop, and destroy go to `net_iface`, which calls the Thread ops. `thread_ops_start()` registers `eventfd` and spawns `ot_task_worker`, which initializes the OpenThread stack and runs the main loop until `thread_ops_stop()` signals `esp_openthread_mainloop_exit()`.

Happy path: `INIT` → `STARTING` → `STARTED` → `CONNECTING` → `READY` (via `OPENTHREAD_EVENT_ATTACHED`).

### Flows 1–4: create through ready

```mermaid
sequenceDiagram
    participant App as User Application
    participant Thread as OpenThread (thread_connect.c)
    participant Core as net_iface Core
    participant Task as Startup Task
    participant Worker as OT Worker (ot_task_worker)
    participant IDF as ESP-IDF OpenThread / netif
    participant Loop as Default Event Loop

    rect 1. Create handle (UNINIT -> INIT)
    App->>Thread: net_thread_create()
    Thread->>Thread: Fill defaults (route_prio = 16, auto_connect_on_start = true), apply params
    Thread->>Core: net_iface_init_with_ops(g_net_iface_thread_ops)
    Core->>Core: Allocate handle, state = INIT (ops->init is NULL)
    opt Event loop exists and global handlers not registered yet
        Core->>Loop: Register (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (OPENTHREAD_EVENT, ESP_EVENT_ANY_ID) -> on_global_ot_event
        Loop-->>Core: Return ESP_OK
    end
    Core-->>Thread: Return handle
    Thread-->>App: Return handle (ESP_OK)
    end

    rect 2. Start interface (INIT -> STARTING)
    App->>Core: net_iface_start()
    opt Global handlers not registered in step 1 (event loop created after handle)
        Core->>Loop: Register (NET_IFACE_EVENT, ESP_EVENT_ANY_ID) -> on_global_iface_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (IP_EVENT, ESP_EVENT_ANY_ID) -> on_global_ip_event
        Loop-->>Core: Return ESP_OK
        Core->>Loop: Register (OPENTHREAD_EVENT, ESP_EVENT_ANY_ID) -> on_global_ot_event
        Loop-->>Core: Return ESP_OK
    end
    Core->>Core: Clear liveness and STARTUP_DONE bits, state = STARTING
    Core->>Task: xTaskCreate(iface_startup_task)
    Core-->>App: Return ESP_OK immediately
    end

    rect 3. Driver start (STARTING -> spawns ot_task_worker)
    Task->>Thread: ops->start() (thread_ops_start)
    Thread->>Thread: Create/clear s_ot_stop_sem, s_ot_stop_requested = false
    Thread->>IDF: esp_vfs_eventfd_register()
    IDF-->>Thread: Return ESP_OK
    Thread->>Loop: Register (OPENTHREAD_EVENT, OPENTHREAD_EVENT_SET_DNS_SERVER) -> on_thread_dns_server
    Loop-->>Thread: Return ESP_OK
    Thread->>Worker: xTaskCreate(ot_task_worker, "ot_br_main")
    Thread-->>Task: Return ESP_OK from thread_ops_start()
    Task->>Core: Save stack HWM, set STARTUP_DONE
    Core-->>Task: Done
    Task->>Task: vTaskDelete(NULL)
    end

    rect 4. OT worker initializes stack and auto-connects (STARTING -> STARTED -> CONNECTING)
    Worker->>IDF: esp_netif_new() and esp_openthread_init(&config)
    IDF-->>Worker: Return ESP_OK
    Worker->>IDF: esp_openthread_netif_glue_init(&config) and esp_netif_attach()
    IDF-->>Worker: Return ESP_OK
    Worker->>IDF: Acquire OT lock, otLoggingSetLevel(), esp_openthread_cli_init/create_task(), release OT lock
    IDF-->>Worker: Return
    Worker->>Core: net_iface_notify_started(s_openthread_netif)
    Core->>Core: Save netif, state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_STARTED
    Loop-->>Core: Queued
    Core-->>Worker: Return from net_iface_notify_started()
    opt auto_connect_on_start is true and state == STARTED
        Worker->>Core: net_iface_connect()
        Core->>Core: Clear READY/FAILED bits, state = CONNECTING
        Core->>Thread: ops->connect() (thread_ops_connect)
        Thread->>IDF: Acquire OT lock, otDatasetGetActiveTlvs(), esp_openthread_auto_start(), release OT lock
        IDF-->>Thread: Return ESP_OK
        Thread-->>Core: Return ESP_OK from thread_ops_connect()
        Core-->>Worker: Return ESP_OK from net_iface_connect()
    end
    Worker->>IDF: esp_openthread_launch_mainloop() (blocks while Thread runs)
    end

    rect 5. Mesh attached (CONNECTING -> READY)
    IDF->>Loop: Post OPENTHREAD_EVENT_ATTACHED
    Loop->>Core: on_global_ot_event(OPENTHREAD_EVENT_ATTACHED)
    Core->>Core: net_iface_get_by_type(NET_IFACE_TYPE_THREAD) -> set OT_ATTACHED bit
    Core->>Core: is_ready_satisfied_locked() is true -> net_iface_notify_ready()
    Core->>Core: state = READY, set READY bit, pulse global wait
    Core->>Loop: Post NET_IFACE_EVENT_READY
    Loop-->>Core: Queued
    Core-->>Loop: Return from on_global_ot_event()
    end

    rect 6. Wait for readiness (READY)
    App->>Core: net_iface_wait(timeout)
    Core-->>App: READY bit is set -> return ESP_OK
    end
```

### Flow 7: disconnect, stop, and destroy

`net_iface_disconnect()` disables Thread participation (`otThreadSetEnabled(instance, false)`) while leaving `ot_task_worker` running in its main loop. `net_iface_stop()` disables Thread and IPv6, calls `esp_openthread_mainloop_exit()`, and waits on `s_ot_stop_sem` for `ot_task_worker` to deinitialize the netif, glue, OpenThread stack, and `eventfd` on its own task before deleting itself.

```mermaid
sequenceDiagram
    participant App as User Application
    participant Thread as OpenThread (thread_connect.c)
    participant Core as net_iface Core
    participant Worker as OT Worker (ot_task_worker)
    participant IDF as ESP-IDF OpenThread / netif
    participant Loop as Default Event Loop

    rect 1. Disconnect (READY/CONNECTING -> STARTED)
    App->>Core: net_iface_disconnect()
    Core->>Thread: ops->disconnect() (thread_ops_disconnect)
    Thread->>IDF: Acquire OT lock, otThreadSetEnabled(instance, false), release OT lock
    IDF-->>Thread: Return OT_ERROR_NONE
    Thread-->>Core: Return ESP_OK from thread_ops_disconnect()
    Core->>Core: net_iface_notify_disconnected() -> state = STARTED
    Core->>Loop: Post NET_IFACE_EVENT_DISCONNECTED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK (ot_task_worker keeps running)
    end

    rect 2. Stop driver and cooperative mainloop exit (STARTED -> STOPPING -> INIT)
    App->>Core: net_iface_stop()
    Core->>Core: state = STOPPING (wait for STARTUP_DONE if startup task is running)
    Core->>Thread: ops->disconnect() (thread_ops_disconnect)
    Thread->>IDF: otThreadSetEnabled(instance, false)
    IDF-->>Thread: Return
    Thread-->>Core: Return ESP_OK
    Core->>Thread: ops->stop() (thread_ops_stop)
    Thread->>Loop: Unregister (OPENTHREAD_EVENT, OPENTHREAD_EVENT_SET_DNS_SERVER) -> on_thread_dns_server
    Loop-->>Thread: Return ESP_OK
    Thread->>Thread: s_ot_stop_requested = true
    Thread->>IDF: Acquire OT lock, otThreadSetEnabled(false), otIp6SetEnabled(false), release OT lock
    IDF-->>Thread: Return
    Thread->>IDF: esp_openthread_mainloop_exit()
    IDF-->>Worker: Unblock esp_openthread_launch_mainloop()
    Note over App,Thread: App task waits on s_ot_stop_sem (up to 5 s)
    Worker->>IDF: esp_openthread_netif_glue_deinit(), esp_netif_destroy(s_openthread_netif)
    IDF-->>Worker: Return
    Worker->>IDF: esp_openthread_deinit(), esp_vfs_eventfd_unregister()
    IDF-->>Worker: Return
    Worker->>Thread: xSemaphoreGive(s_ot_stop_sem) (ot_signal_task_exited)
    Worker->>Worker: vTaskDelete(NULL)
    Thread->>Thread: vSemaphoreDelete(s_ot_stop_sem)
    Thread-->>Core: Return ESP_OK from thread_ops_stop()
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
        Core->>Loop: Unregister (OPENTHREAD_EVENT, ESP_EVENT_ANY_ID) -> on_global_ot_event
        Loop-->>Core: Return
    end
    Note over Core,Thread: ops->deinit is NULL for OpenThread
    Core->>Core: Free event group, mutex, and handle (state = UNINIT)
    Core-->>App: Return ESP_OK
    end
```

### Flow 5, worker failure, and stop before mainloop entry

Manual connect (`auto_connect_on_start = false`), stack initialization failure inside `ot_task_worker`, and a stop that arrives before `ot_task_worker` enters `esp_openthread_launch_mainloop()`.

```mermaid
sequenceDiagram
    participant App as User Application
    participant Thread as OpenThread (thread_connect.c)
    participant Core as net_iface Core
    participant Worker as OT Worker (ot_task_worker)
    participant IDF as ESP-IDF OpenThread / netif
    participant Loop as Default Event Loop

    rect Flow 5 - Start without auto-connect, connect later (INIT -> STARTED -> CONNECTING)
    App->>Thread: 1. net_thread_create() with auto_connect_on_start = false
    Thread->>Core: net_iface_init_with_ops(g_net_iface_thread_ops)
    Core-->>Thread: Return handle
    Thread-->>App: Return handle (ESP_OK)
    App->>Core: 2. net_iface_start() -> spawns ot_task_worker
    Core-->>App: Return ESP_OK
    Worker->>Core: net_iface_notify_started(s_openthread_netif) -> state = STARTED
    Core-->>Worker: Return
    Note over Worker,Core: auto_connect_on_start is false, connect is skipped
    Worker->>IDF: esp_openthread_launch_mainloop()
    App->>Core: 3. net_iface_connect()
    Core->>Thread: ops->connect() (thread_ops_connect)
    Thread->>IDF: Acquire OT lock, thread_auto_start_locked(), release OT lock
    IDF-->>Thread: Return ESP_OK
    Thread-->>Core: Return ESP_OK from thread_ops_connect()
    Core-->>App: Return ESP_OK from net_iface_connect()
    end

    rect Flow 6 - Initialization failure inside ot_task_worker (STARTING -> FAILED)
    Worker->>IDF: 4. esp_netif_new(), esp_openthread_init(), or esp_openthread_netif_glue_init() fails
    IDF-->>Worker: Return error
    Worker->>IDF: Clean up partial resources (netif, OT stack, eventfd)
    IDF-->>Worker: Return
    Worker->>Core: net_iface_notify_failed(self, err)
    Core->>Core: state = FAILED, set FAILED bit, wake waiters
    Core->>Loop: Post NET_IFACE_EVENT_FAILED
    Loop-->>Core: Queued
    Core-->>Worker: Return from net_iface_notify_failed()
    Worker->>Thread: ot_signal_task_exited() (give s_ot_stop_sem)
    Worker->>Worker: vTaskDelete(NULL)
    end

    rect Flow 7 - Stop before ot_task_worker enters esp_openthread_launch_mainloop()
    Note over App,Worker: 5a. App calls net_iface_stop() before Worker reaches launch_mainloop()
    App->>Core: net_iface_stop()
    Core->>Thread: ops->stop() (thread_ops_stop)
    Thread->>Thread: s_ot_stop_requested = true
    Note over App,Thread: App task waits on s_ot_stop_sem (up to 5 s)

    Note over Worker,IDF: 5b. Worker sees s_ot_stop_requested == true, skips mainloop, and cleans up
    Worker->>Worker: Check s_ot_stop_requested == true -> skip esp_openthread_launch_mainloop()
    Worker->>IDF: thread_release_netif(), esp_openthread_deinit(), thread_release_eventfd()
    IDF-->>Worker: Return
    Worker->>Thread: ot_signal_task_exited() (wakes App task on s_ot_stop_sem)
    Worker->>Worker: vTaskDelete(NULL)

    Note over App,Loop: 5c. App task wakes up, deletes s_ot_stop_sem, and finishes stop()
    Thread->>Thread: vSemaphoreDelete(s_ot_stop_sem)
    Thread-->>Core: Return ESP_OK from thread_ops_stop()
    Core->>Core: net_iface_notify_stopped() -> state = INIT, set STOPPED bit
    Core->>Loop: Post NET_IFACE_EVENT_STOPPED
    Loop-->>Core: Queued
    Core-->>App: Return ESP_OK from net_iface_stop()
    end
```

---

## The objects the driver keeps

OpenThread fills the ops table in `thread_connect.c` (lines 348–356):

```c
const net_iface_ops_t g_net_iface_thread_ops = {
    .name = "thread",
    .init = NULL,
    .deinit = NULL,
    .start = thread_ops_start,
    .stop = thread_ops_stop,
    .connect = thread_ops_connect,
    .disconnect = thread_ops_disconnect,
};
```

Module-level state in `thread_connect.c` (lines 39–50):

```c
static TaskHandle_t s_ot_task_handle = NULL;
static SemaphoreHandle_t s_ot_stop_sem = NULL;
static volatile bool s_ot_stop_requested = false;
static volatile bool s_ot_task_published = false;
static esp_netif_t *s_openthread_netif = NULL;
static bool s_eventfd_owned = false;
static bool s_glue_attached = false;
```

- `s_ot_task_handle` tracks the `"ot_br_main"` task running `ot_task_worker`.
- `s_ot_stop_sem` is a binary semaphore signaled by `ot_signal_task_exited()` right before `ot_task_worker` calls `vTaskDelete(NULL)`.
- `s_ot_task_published` guards against a fast exit where `ot_task_worker` finishes before `xTaskCreate()` in `thread_ops_start()` has written `s_ot_task_handle`.
- `s_eventfd_owned` records whether `thread_ops_start()` registered `eventfd` (`ESP_OK`) or whether the application had already registered it (`ESP_ERR_INVALID_STATE`), so `thread_release_eventfd()` only unregisters what this driver installed.

---

## Flow 1: Create the handle

Public call: `net_thread_create()` in `thread_connect.c` (lines 358–394):

1. Builds `net_iface_config_t`:
   - `type = NET_IFACE_TYPE_THREAD`
   - `desc = NET_CONNECT_NETIF_DESC_THREAD` (`"net_connect_netif_thread"`)
   - `route_prio = NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO` (16)
   - `required` from `CONFIG_NET_CONNECT_THREAD_REQUIRED`
   - `auto_connect_on_start = true`
   - `ip.enable_ipv6 = true`
   - `thread.task_stack_size = CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE`
2. A non-NULL `net_thread_params_t` overrides `task_stack_size`, `route_prio`, `required`, `auto_connect_on_start`, and `ip`.
3. Calls `net_iface_init_with_ops(&cfg, &g_net_iface_thread_ops, out_handle)`. When `CONFIG_NET_CONNECT_THREAD` is enabled, `register_global_handlers()` in `net_iface.c` (lines 207–220) registers `(OPENTHREAD_EVENT, ESP_EVENT_ANY_ID)` → `on_global_ot_event` alongside `NET_IFACE_EVENT` and `IP_EVENT`.

---

## Flow 2: `net_iface_start()`, `thread_ops_start()`, and `ot_task_worker`

`net_iface_start()` spawns `iface_startup_task`, which calls `thread_ops_start()` (`thread_connect.c`, lines 225–277):

1. Rejects a second start if `s_ot_task_handle != NULL` (`ESP_ERR_INVALID_STATE`).
2. Creates `s_ot_stop_sem` (or clears any stale token with `xSemaphoreTake(s_ot_stop_sem, 0)`), and resets `s_ot_stop_requested = false` and `s_ot_task_published = false`.
3. Registers `eventfd` with `esp_vfs_eventfd_register(&eventfd_config)` (`max_fds = 4`).
4. Registers `(OPENTHREAD_EVENT, OPENTHREAD_EVENT_SET_DNS_SERVER)` → `on_thread_dns_server`.
5. Spawns `ot_task_worker` (`"ot_br_main"`, priority 5) and sets `s_ot_task_published = true`.

Note that `thread_ops_start()` returns `ESP_OK` as soon as `ot_task_worker` is created; `net_iface_notify_started()` is called inside `ot_task_worker` once the OpenThread stack and netif glue are initialized. Because `iface_startup_task` may see `state == STARTING` when `thread_ops_start()` returns, `ot_task_worker` also checks `auto_connect_on_start` after calling `net_iface_notify_started()`. Calling `net_iface_connect()` only when `net_iface_get_state(self) == NET_IFACE_STATE_STARTED` prevents a double connect if both tasks race (`thread_connect.c`, lines 196–203).

Inside `ot_task_worker()` (`thread_connect.c`, lines 130–222):

1. Creates `s_openthread_netif` with `ESP_NETIF_INHERENT_DEFAULT_OPENTHREAD()` and `g_esp_netif_netstack_default_openthread`.
2. Calls `esp_openthread_init(&config)`.
3. Attaches the netif glue via `esp_openthread_netif_glue_init(&config)` and `esp_netif_attach()`.
4. Under `esp_openthread_lock_acquire()`, configures logging and initializes the OpenThread CLI (`esp_openthread_cli_init()`, `esp_openthread_cli_create_task()`).
5. Calls `net_iface_notify_started(self, s_openthread_netif)` (`STARTING` → `STARTED`).
6. If `auto_connect_on_start` is true and the state is still `STARTED`, calls `net_iface_connect(self)` → `thread_ops_connect()` (`thread_connect.c`, lines 280–291), which acquires the OpenThread lock, reads active operational dataset TLVs (`otDatasetGetActiveTlvs`), and calls `esp_openthread_auto_start()`.
7. If `!s_ot_stop_requested`, enters `esp_openthread_launch_mainloop()`, blocking there for the lifetime of the Thread session.

---

## Flow 3: Mesh attachment and `READY`

Unlike Wi-Fi, Ethernet, and PPP, OpenThread does not use `net_iface_notify_connected()` or `IP_EVENT_GOT_IP6` to become `READY`:

1. When the node attaches to the Thread partition, ESP-IDF OpenThread posts `OPENTHREAD_EVENT_ATTACHED` on the default event loop.
2. `on_global_ot_event()` (`net_iface.c`, lines 702–714) looks up the Thread handle with `net_iface_get_by_type(NET_IFACE_TYPE_THREAD)` and calls `set_readiness_bit(..., NET_IFACE_BIT_OT_ATTACHED)`.
3. `is_ready_satisfied_locked()` (`net_iface.c`, lines 363–366) checks only `NET_IFACE_BIT_OT_ATTACHED` for `NET_IFACE_TYPE_THREAD`:

```c
static bool is_ready_satisfied_locked(net_iface_handle_t h)
{
    EventBits_t bits = xEventGroupGetBits(h->state_events);
    if (h->cfg.type == NET_IFACE_TYPE_THREAD) {
        return (bits & NET_IFACE_BIT_OT_ATTACHED) != 0;
    }
    // ...
}
```

4. `net_iface_notify_ready()` transitions the handle to `READY`, sets `NET_IFACE_BIT_READY`, pulses `s_global_wait_events`, and posts `NET_IFACE_EVENT_READY`.

---

## Flow 7: Disconnect, cooperative stop, and destroy

- **Leave the Thread network only (`net_iface_disconnect`):** `thread_ops_disconnect()` (`thread_connect.c`, lines 294–309) acquires the OpenThread lock and calls `otThreadSetEnabled(instance, false)`. `net_iface_notify_disconnected()` moves the handle back to `STARTED` and clears `NET_IFACE_BITS_LIVENESS`. `ot_task_worker` remains inside `esp_openthread_launch_mainloop()`, so a later `net_iface_connect()` can rejoin with `thread_auto_start_locked()`.
- **Stop the OpenThread stack (`net_iface_stop`):** `thread_ops_stop()` (`thread_connect.c`, lines 315–345) never calls `vTaskDelete()` on `ot_task_worker` from outside, because OpenThread holds internal state and locks that must be released on the worker thread:
  1. Unregisters `on_thread_dns_server`.
  2. Sets `s_ot_stop_requested = true`.
  3. If the OpenThread instance exists, acquires the OpenThread lock, calls `otThreadSetEnabled(instance, false)` and `otIp6SetEnabled(instance, false)`, releases the lock, and calls `esp_openthread_mainloop_exit()`.
  4. Waits up to 5000 ms on `s_ot_stop_sem`.
  5. `ot_task_worker` returns from `esp_openthread_launch_mainloop()`, tears down its resources in reverse order (`thread_release_netif()` → `esp_openthread_deinit()` → `thread_release_eventfd()`), gives `s_ot_stop_sem` via `ot_signal_task_exited()`, and deletes itself.
  6. `thread_ops_stop()` deletes `s_ot_stop_sem`, and `net_iface_notify_stopped()` transitions the handle to `INIT`.
- **Free the handle (`net_iface_deinit`):** Unlinks the handle in `INIT`, unregisters the shared `NET_IFACE_EVENT`, `IP_EVENT`, and `OPENTHREAD_EVENT` handlers when the registry becomes empty, and frees the handle.
