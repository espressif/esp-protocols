# Changelog

## [0.2.0](https://github.com/espressif/esp-protocols/commits/net_connect-v0.2.0)

### Features

- Add non-blocking, handle-based interface lifecycle API (`include/net_iface.h`): `net_iface_init()`, `net_iface_deinit()`, `net_iface_start()`, `net_iface_stop()`, `net_iface_connect()`, `net_iface_disconnect()`, and typed creation wrappers (`net_wifi_sta_create()`, `net_eth_create()`, `net_thread_create()`, `net_ppp_create()`)
- Add per-interface background startup tasks so slow driver initialization (such as Ethernet PHY autonegotiation when unplugged) never blocks other interfaces from starting and connecting concurrently
- Add shared readiness state machine (`NET_IFACE_STATE_*`) and event notifications (`NET_IFACE_EVENT_*`) on the default ESP event loop
- Add blocking wait helpers (`net_iface_wait()`, `net_iface_wait_all()`, `net_iface_wait_any()`) with shared timeout budgets and event-loop deadlock protection
- Add `net_wifi_sta_set_credentials()` for runtime Wi-Fi provisioning on started, unprovisioned handles
- Add `CONFIG_NET_CONNECT_*_REQUIRED` and `CONFIG_NET_CONNECT_TIMEOUT_MS` Kconfig options so `net_connect()` supports required and optional interfaces concurrently while remaining backward-compatible with `0.1.x`

### Bug Fixes

- Replace `ESP_ERROR_CHECK()` aborts in library paths with graceful `esp_err_t` error returns and cleanup
- Replace unsafe `vTaskDelete()` in OpenThread shutdown with cooperative `esp_openthread_mainloop_exit()`
- Fix PPP UART task shutdown wait and make PPP/startup task stack sizes configurable in Kconfig
- Add 31-test Unity suite (`tests/unit_test`) covering concurrency, route priority, cancellation, stale events, stack headroom, and memory leaks

## [0.1.1](https://github.com/espressif/esp-protocols/commits/net_connect-v0.1.1)

### Updated

- ci(net_connect): fix CI failures for component registry and build ([f869d05d](https://github.com/espressif/esp-protocols/commit/f869d05d))

## [0.1.0](https://github.com/espressif/esp-protocols/commits/net_connect-v0.1.0)

### Features

- Add network connectivity component ([ac2c8385](https://github.com/espressif/esp-protocols/commit/ac2c8385))
