# Net Connect: Non-blocking Start-and-Wait Example

This example demonstrates the non-blocking API of the `net_connect` component on an ESP32 board with Wi-Fi and a built-in Ethernet PHY (the `ethernet_init` defaults: internal EMAC, IP101, MDC GPIO 23, MDIO GPIO 18).

## Overview

The example runs in two parts:

1. **One interface (Wi-Fi)** — start one station and wait for it.
2. **Two interfaces (Ethernet and Wi-Fi)** — start both links at once, use the first one that becomes ready, then wait for both.

### Key idea

All control calls (`net_iface_create*`, `net_iface_start()`, `net_iface_stop()`, `net_iface_deinit()`) are **non-blocking** — they return immediately while the interface work happens on a background task. The application only blocks when it explicitly calls one of the wait functions:

| Blocking call | Waits until |
|---|---|
| `net_iface_wait()` | One named interface is ready |
| `net_iface_wait_any()` | The first of several interfaces is ready |
| `net_iface_wait_all()` | All interfaces are ready (or one has failed) |
| `getaddrinfo()` | DNS resolution completes |

There are no event callbacks and no extra application tasks — everything stays in `app_main()`. Where the example prints `blocked N ms`, that is the measured time the call held the application task.

## What the Example Does

### Part 1: Single interface (Wi-Fi)

1. `net_wifi_sta_create()` and `net_iface_start()` return while the station associates on a background task.
2. The application sleeps one second, three times, printing the Wi-Fi state on each step. The state advances from `STARTING` through `CONNECTING` to `CONNECTED` while the application keeps running.
3. When the application is ready to use the network, it calls `net_iface_wait()`.
4. On success, it prints the IPv4 address, resolves `example.com`, and stays connected for 3 seconds.
5. `net_iface_stop()` and `net_iface_deinit()` release the interface.

A failed wait is logged and the example continues, so a missing access point does not hide the rest of the demo.

### Part 2: Ethernet and Wi-Fi together

1. Both handles are created and both `net_iface_start()` calls return immediately, so the two links come up in parallel.
2. The same three one-second application steps run while both links connect.
3. `net_iface_wait_any()` returns as soon as the **first** interface is ready, and the application uses that interface.
4. `net_iface_wait_all()` then waits until both are ready, or returns the error from an interface that failed.
5. Both handles are stopped and released.

## Build and Flash

Plug the Ethernet cable into a network with a DHCP server, set the Wi-Fi SSID and password via `idf.py menuconfig` under **Net Connect Configuration**, then:

```bash
cd components/net_connect/examples/net_iface_example
idf.py set-target esp32
idf.py -p /dev/tty.usbserial-1132201 flash monitor
```

## Sample Output (ESP32, `/dev/tty.usbserial-1132201`)

Test setup: Ethernet cable connected; DHCP assigned Ethernet `192.168.10.11` (mask `255.255.255.0`, gateway `192.168.10.1`) and Wi-Fi `192.168.50.47` (mask `255.255.255.0`, gateway `192.168.50.1`). IP101 on the internal EMAC (MDC 23, MDIO 18). Wi-Fi joined the SSID set in Kconfig.

**Part 1.** After `net_iface_start()`, the interface was `STARTING`. The three application steps ran while the station moved from `CONNECTING` to `CONNECTED`:

- `net_iface_wait(wifi)` blocked for **962 ms** and returned `ESP_OK`.
- Wi-Fi address: `192.168.50.47`.
- `getaddrinfo("example.com")` resolved to `172.66.147.243` in **14 ms**.

**Part 2.** After both `net_iface_start()` calls, Ethernet and Wi-Fi were `STARTING`, and the application steps ran while both came up:

| Loop | Ethernet | Wi-Fi |
|---|---|---|
| 1 | CONNECTING | CONNECTING |
| 2 | CONNECTING | CONNECTING |
| 3 | CONNECTED | CONNECTED |

- `net_iface_wait_any()` blocked for **921 ms** and named Wi-Fi as the first ready interface.
- Ethernet received `192.168.10.11` during the 3-second hold, so `net_iface_wait_all()` returned `ESP_OK` in **0 ms** with both interfaces `READY`.

### Full log

```
I (548) net_iface_example: Wi-Fi SSID from Kconfig: Avrow
I (558) net_iface_example: ===== Part 1: single interface (Wi-Fi) =====
I (588) net_iface_example: Wi-Fi state after start: STARTING
I (758) net_connect_wifi: Connecting to Avrow...
I (1588) net_iface_example: [app] startup work 1/3, Wi-Fi state: CONNECTING
I (2588) net_iface_example: [app] startup work 2/3, Wi-Fi state: CONNECTING
I (3358) wifi:connected with Avrow, aid = 30, channel 10, BW20, bssid = a0:36:bc:0e:c4:f0
I (3588) net_iface_example: [app] startup work 3/3, Wi-Fi state: CONNECTED
I (4398) esp_netif_handlers: net_connect_netif_sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (4548) net_iface_example: net_iface_wait(wifi) blocked 962 ms
I (4548) net_iface_example: net_iface_wait() returned ESP_OK, Wi-Fi state: READY
I (4548) net_iface_example: Wi-Fi address 192.168.50.47
I (4568) net_iface_example: getaddrinfo(example.com) -> 172.66.147.243, blocked 14 ms
I (4568) net_iface_example: [app] staying connected for 3 s
I (7628) net_iface_example: ===== Part 2: Ethernet and Wi-Fi together =====
I (7628) net_iface_example: After both starts: Ethernet STARTING, Wi-Fi STARTING
I (8628) net_iface_example: [app] startup work 1/3, Ethernet: CONNECTING, Wi-Fi: CONNECTING
I (9628) net_iface_example: [app] startup work 2/3, Ethernet: CONNECTING, Wi-Fi: CONNECTING
I (9808) ethernet_init: Ethernet(IP101[23,18]) Link Up
I (10338) wifi:connected with Avrow, aid = 30, channel 10, BW20, bssid = a0:36:bc:0e:c4:f0
I (10628) net_iface_example: [app] startup work 3/3, Ethernet: CONNECTED, Wi-Fi: CONNECTED
I (11378) esp_netif_handlers: net_connect_netif_sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (11548) net_iface_example: net_iface_wait_any() blocked 921 ms
I (11548) net_iface_example: First ready interface: Wi-Fi (READY)
I (11548) net_iface_example: Wi-Fi address 192.168.50.47
I (11558) net_iface_example: getaddrinfo(example.com) -> 172.66.147.243, blocked 1 ms
I (11558) net_iface_example: [app] staying connected for 3 s
I (11818) esp_netif_handlers: net_connect_netif_eth ip: 192.168.10.11, mask: 255.255.255.0, gw: 192.168.10.1
I (14568) net_iface_example: net_iface_wait_all() blocked 0 ms
I (14568) net_iface_example: net_iface_wait_all() returned ESP_OK, Ethernet: READY, Wi-Fi: READY
I (14648) net_iface_example: Example finished
```

## Known Notes

- During `net_iface_stop()` on Ethernet, ESP-IDF logs `esp_eth_stop: driver not started yet`. Disconnect already stopped the driver, and the release path stops it again. `net_iface_stop()` still returns, and `net_iface_deinit()` completes. The message is harmless.
