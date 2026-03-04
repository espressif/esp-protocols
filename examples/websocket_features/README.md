# WebSocket Feature Showcase Example

## Overview

This example demonstrates the newest `esp_websocket_client` capabilities in one place, with verbose comments in code and runtime logs.

It is designed to be a **reference app** when you want to copy/paste a production-ready websocket control flow and then trim it for your use case.

### Features Demonstrated

1. Event-driven lifecycle handling (`BEGIN`, `BEFORE_CONNECT`, `CONNECTED`, `DATA`, `ERROR`, `DISCONNECTED`, `CLOSED`, `FINISH`)
2. Header inspection via `WEBSOCKET_EVENT_HEADER_RECEIVED` (IDF 6+)
3. Custom headers using both:
   - `esp_websocket_client_set_headers()`
   - `esp_websocket_client_append_header()`
4. Fragmented transfer APIs:
   - `esp_websocket_client_send_text_partial()`
   - `esp_websocket_client_send_bin_partial()`
   - `esp_websocket_client_send_cont_msg()`
   - `esp_websocket_client_send_fin()`
5. Runtime reconnect tuning:
   - `esp_websocket_client_get_reconnect_timeout()`
   - `esp_websocket_client_set_reconnect_timeout()`
6. Runtime ping tuning:
   - `esp_websocket_client_get_ping_interval_sec()`
   - `esp_websocket_client_set_ping_interval_sec()`
7. Task-preserving reconnect with:
   - `esp_websocket_client_pause()`
   - `esp_websocket_client_set_uri()`
   - `esp_websocket_client_resume()`
8. Close handshake and clean teardown:
   - `esp_websocket_client_close()`
   - unregister events
   - `esp_websocket_client_destroy()`

## Project Layout

- `main/app_main.c` — fully commented example source.
- `main/Kconfig.projbuild` — URI configuration in menuconfig.
- `main/idf_component.yml` — dependencies.

## Configure

Set your endpoint in menuconfig:

```bash
idf.py menuconfig
```

Path:

```text
WebSocket Feature Showcase Configuration  --->
    WebSocket endpoint URI
```

Default endpoint:

```text
wss://echo.websocket.events
```

## Build & Flash

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## What to look for in logs

- Upgrade headers printed by `WEBSOCKET_EVENT_HEADER_RECEIVED`.
- Text + binary + fragmented send sequences.
- Ping/reconnect values before and after runtime update.
- Pause/resume flow reconnecting without destroying the task.

## Notes

- If your server uses TLS with a private CA, configure certificates as needed for your deployment.
- Redirect handling is managed by the websocket transport layer; this example focuses on application-side controls and observability.
