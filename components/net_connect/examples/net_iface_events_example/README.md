# Net Connect: Event-Driven Interface Example

This example drives a Wi-Fi station entirely through `NET_IFACE_EVENT` callbacks. It never calls `net_connect()` or `net_iface_wait*()`.

While `net_iface_example` keeps the whole demo on `app_main()` and blocks on `net_iface_wait*()`, this example is the pattern for an application that cannot sit in `app_main()`: setup returns immediately, and a worker task reacts to events.

## Overview

`app_main()` initializes NVS, `esp_netif`, and the default event loop, creates a command queue, registers `on_net_iface_event()`, starts `app_worker_task`, and returns.

The callback runs on the default event loop task, so it only does work that returns at once. Socket calls and teardown run on the worker, which receives commands with `xQueueReceive()`. The callback posts those commands with `xQueueSend(..., 0)` so a full queue cannot stall the event loop.

| Safe inside `on_net_iface_event()` | Runs on `app_worker_task` |
|---|---|
| Read `net_iface_event_data_t` | `getaddrinfo()` |
| Read the IPv4 address from `data->netif` | Periodic network checks |
| `net_wifi_sta_set_credentials()` | `net_iface_disconnect()` |
| `net_iface_connect()` | `net_iface_stop()` and `net_iface_deinit()` |
| `xQueueSend(..., 0)` | |

### Design rules

- `net_iface_stop()` and `net_iface_wait*()` return `ESP_ERR_INVALID_STATE` when called from the default event loop task. The worker is the task that stops and releases the interface.
- Each part calls `xQueueReset()` before `net_iface_start()`. `net_iface_stop()` disconnects first and can post `NET_IFACE_EVENT_DISCONNECTED` on the way to `STOPPED`. Clearing the queue drops those leftover commands so they cannot wake the next part.
- `.max_retry` and `.route_prio` use `-1` when the Kconfig retry count and the default Wi-Fi route priority (`128`) should apply. `0` is a real override. Part 2 sets `.max_retry = 1` so the bad SSID fails after one retry (two join attempts). `net_wifi_sta_set_credentials()` changes the SSID and password and leaves `max_retry` as it is, so the recovery join uses the same limit.

## What the Example Does

### Part 1: Deferred provisioning

1. The worker creates a station with an empty SSID and `auto_connect_on_start = false`, then calls `net_iface_start()`. The call returns while the driver is still coming up.
2. `NET_IFACE_EVENT_STARTED` runs on the event loop. The callback stores `CONFIG_NET_CONNECT_WIFI_SSID` and `CONFIG_NET_CONNECT_WIFI_PASSWORD` with `net_wifi_sta_set_credentials()` and calls `net_iface_connect()`.
3. `NET_IFACE_EVENT_CONNECTED` is followed by `NET_IFACE_EVENT_READY`. The callback logs the IPv4 address and posts `APP_CMD_START_WORK`.
4. The worker resolves `example.com` three times. Between steps it waits on the queue for one second so `APP_CMD_PAUSE_WORK` can stop the loop early.
5. The worker calls `net_iface_stop()` and `net_iface_deinit()`. The event loop logs `NET_IFACE_EVENT_STOPPED`.

### Part 2: Failure recovery and pause on disconnect

1. The worker starts a station aimed at `invalid_ap_demo` with `max_retry = 1` and `auto_connect_on_start = true`.
2. The join fails. `NET_IFACE_EVENT_FAILED` logs `data->err` and `net_iface_get_last_error()`.
3. On that first failure the callback switches the credentials to the Kconfig SSID and calls `net_iface_connect()`. A later failure posts `APP_CMD_FAILED` so the worker tears down instead of retrying forever.
4. `NET_IFACE_EVENT_READY` posts `APP_CMD_START_WORK`. The worker runs two DNS checks, then calls `net_iface_disconnect()` while the driver stays up.
5. `NET_IFACE_EVENT_DISCONNECTED` (and `NET_IFACE_EVENT_LOST_IP` when the lease is dropped) posts `APP_CMD_PAUSE_WORK`. The worker logs that network activity is paused in `STARTED` and does not run steps 3 and 4.
6. The worker calls `net_iface_stop()` and `net_iface_deinit()`, then unregisters the event handler.

### When the network is unreachable

If the Kconfig SSID is also unreachable, Part 1's `NET_IFACE_EVENT_FAILED` posts `APP_CMD_FAILED` and the worker moves on. Part 2 recovers once; the second `FAILED` posts `APP_CMD_FAILED` and the worker still stops and deinits.

## Build and Flash

Set the Wi-Fi SSID and password with `idf.py menuconfig` under **Net Connect Configuration**, then:

```bash
cd components/net_connect/examples/net_iface_events_example
idf.py set-target esp32
idf.py -p /dev/tty.usbserial-1132201 flash monitor
```

`sdkconfig.defaults` enables Wi-Fi only, so the example also builds for other Wi-Fi targets (`esp32c3`, `esp32s3`, `esp32c6`).

## Sample Output (ESP32, `/dev/tty.usbserial-1132201`)

Test setup: Wi-Fi joined the SSID set in Kconfig (`Avrow`). DHCP assigned `192.168.50.47` (mask `255.255.255.0`, gateway `192.168.50.1`). `+N ms` is the time since that part started.

`app_main()` created the worker and returned. The worker's first log is already present when `main_task` prints `Returned from app_main()`.

**Part 1.** `net_iface_start()` returned in **1 ms** with the interface `STARTING`. At **+211 ms** `NET_IFACE_EVENT_STARTED` provisioned `Avrow` and called `net_iface_connect()`.

- `NET_IFACE_EVENT_CONNECTED` at **+2837 ms**.
- `NET_IFACE_EVENT_READY` at **+3988 ms** with `192.168.50.47`.
- `getaddrinfo("example.com")` resolved to `172.66.147.243` on steps `1/3` (**11 ms**), `2/3` (**0 ms**), and `3/3` (**0 ms**).
- `net_iface_stop()` ran at **+7015 ms** and `NET_IFACE_EVENT_STOPPED` followed at **+7047 ms** with `state=INIT`.

**Part 2.** The worker started `invalid_ap_demo` with `max_retry=1`. The driver logged one retry (`1/1`) and stopped after two failed joins. `NET_IFACE_EVENT_FAILED` at **+4959 ms** reported `data->err=ESP_FAIL` and `last_error=ESP_FAIL`. The callback switched to `Avrow` and called `net_iface_connect()`.

- `NET_IFACE_EVENT_CONNECTED` at **+7602 ms**, then `NET_IFACE_EVENT_READY` at **+8922 ms** with `192.168.50.47`.
- Worker steps `1/4` and `2/4` resolved `example.com` in **0 ms**.
- `net_iface_disconnect()` at **+9942 ms** posted `NET_IFACE_EVENT_DISCONNECTED (state=STARTED)`.
- The worker logged `network activity paused (state=STARTED)` at **+9986 ms** and did not run steps `3/4` or `4/4`.
- `NET_IFACE_EVENT_STOPPED` at **+10020 ms**, then `Example finished`.

### Full log

```
I (528) net_iface_events: Target Wi-Fi SSID from Kconfig: Avrow
I (528) net_iface_events:
=============== Part 1: deferred provisioning via NET_IFACE_EVENT_STARTED ===============
I (538) net_iface_events: +1 ms  [worker] net_iface_start(wifi) returned immediately (state=STARTING)
I (568) net_iface_events: app_main() setup complete; returning now while events and worker task run
I (568) main_task: Returned from app_main()
I (748) net_iface_events: +211 ms  [event] Wi-Fi -> NET_IFACE_EVENT_STARTED (state=STARTED)
I (748) net_iface_events: +211 ms  [event] Driver is STARTED; provisioning credentials for 'Avrow' and connecting
I (768) net_connect_wifi: Connecting to Avrow...
I (3348) wifi:connected with Avrow, aid = 30, channel 10, BW20, bssid = a0:36:bc:0e:c4:f0
I (3378) net_iface_events: +2837 ms  [event] Wi-Fi -> NET_IFACE_EVENT_CONNECTED (state=CONNECTED)
I (3378) net_iface_events: +2841 ms  [event] L2 link is up; waiting for IP address...
I (4398) esp_netif_handlers: net_connect_netif_sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (4528) net_iface_events: +3988 ms  [event] Wi-Fi -> NET_IFACE_EVENT_READY (state=READY)
I (4528) net_iface_events: +3989 ms  [event] Wi-Fi is READY with IP 192.168.50.47; notifying worker task
I (4548) net_iface_events: +4006 ms  [worker] step 1/3 (Wi-Fi=READY): getaddrinfo(example.com) -> 172.66.147.243 (blocked 11 ms)
I (5558) net_iface_events: +5016 ms  [worker] step 2/3 (Wi-Fi=READY): getaddrinfo(example.com) -> 172.66.147.243 (blocked 0 ms)
I (6558) net_iface_events: +6016 ms  [worker] step 3/3 (Wi-Fi=READY): getaddrinfo(example.com) -> 172.66.147.243 (blocked 0 ms)
I (7558) net_iface_events: +7015 ms  [worker] Stopping and deinitializing Wi-Fi from worker task
I (7588) net_iface_events: +7047 ms  [event] Wi-Fi -> NET_IFACE_EVENT_STOPPED (state=INIT)
I (7588) net_iface_events: +7048 ms  [event] Wi-Fi STOPPED (netif destroyed, state=INIT)
I (7598) net_iface_events:
=============== Part 2: FAILED recovery and pause on DISCONNECTED ===============
I (7608) net_iface_events: +1 ms  [worker] Started Wi-Fi with bad SSID 'invalid_ap_demo' (max_retry=1, state=STARTING)
I (7728) net_connect_wifi: Connecting to invalid_ap_demo...
I (7728) net_iface_events: +121 ms  [event] Wi-Fi -> NET_IFACE_EVENT_STARTED (state=STARTING)
I (10148) net_connect_wifi: Wi-Fi disconnected, trying to reconnect... (1/1)
I (12558) net_connect_wifi: WiFi Connect failed 2 times, stop reconnect.
I (12568) net_iface_events: +4959 ms  [event] Wi-Fi -> NET_IFACE_EVENT_FAILED (state=FAILED)
W (12568) net_iface_events: +4963 ms  [event] Wi-Fi FAILED (data->err=ESP_FAIL, last_error=ESP_FAIL)
I (12578) net_iface_events: +4972 ms  [event] Recovering from FAILED: switching to 'Avrow' and calling net_iface_connect()
I (12598) net_connect_wifi: Connecting to Avrow...
I (15188) wifi:connected with Avrow, aid = 30, channel 10, BW20, bssid = a0:36:bc:0e:c4:f0
I (15208) net_iface_events: +7602 ms  [event] Wi-Fi -> NET_IFACE_EVENT_CONNECTED (state=CONNECTED)
I (15208) net_iface_events: +7607 ms  [event] L2 link is up; waiting for IP address...
I (16228) esp_netif_handlers: net_connect_netif_sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (16528) net_iface_events: +8922 ms  [event] Wi-Fi -> NET_IFACE_EVENT_READY (state=READY)
I (16528) net_iface_events: +8923 ms  [event] Wi-Fi is READY with IP 192.168.50.47; notifying worker task
I (16538) net_iface_events: +8930 ms  [worker] step 1/4 (Wi-Fi=READY): getaddrinfo(example.com) -> 172.66.147.243 (blocked 0 ms)
I (17548) net_iface_events: +9939 ms  [worker] step 2/4 (Wi-Fi=READY): getaddrinfo(example.com) -> 172.66.147.243 (blocked 0 ms)
I (17548) net_iface_events: +9942 ms  [worker] Calling net_iface_disconnect() to simulate link loss...
I (17578) net_iface_events: +9969 ms  [event] Wi-Fi -> NET_IFACE_EVENT_DISCONNECTED (state=STARTED)
I (17578) net_iface_events: +9976 ms  [event] Link/IP lost (DISCONNECTED); telling worker task to pause network I/O
I (17588) net_iface_events: +9986 ms  [worker] Received pause command; network activity paused (state=STARTED)
I (17598) net_iface_events: +9996 ms  [worker] Stopping and deinitializing Wi-Fi from worker task
I (17628) net_iface_events: +10020 ms  [event] Wi-Fi -> NET_IFACE_EVENT_STOPPED (state=INIT)
I (17628) net_iface_events: +10024 ms  [event] Wi-Fi STOPPED (netif destroyed, state=INIT)
I (17638) net_iface_events: Example finished
```

## Known Notes

- This disconnect posted `NET_IFACE_EVENT_DISCONNECTED` and did not post `NET_IFACE_EVENT_LOST_IP`. The pause command is the same for both events.
