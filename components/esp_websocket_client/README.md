# ESP WEBSOCKET CLIENT

[![Component Registry](https://components.espressif.com/components/espressif/esp_websocket_client/badge.svg)](https://components.espressif.com/components/espressif/esp_websocket_client)

The `esp-websocket_client` component is a managed component for `esp-idf` that contains an implementation of the [WebSocket protocol client](https://datatracker.ietf.org/doc/html/rfc6455) for ESP targets.

## Highlights

- WebSocket over TCP (`ws`) and TLS (`wss`)
- Optional header callback event (`WEBSOCKET_EVENT_HEADER_RECEIVED`, IDF 6+)
- Fragmented message send helpers for text and binary payloads
- Runtime ping/reconnect tuning APIs
- Pause/resume support without destroying the websocket task
- Automatic HTTP redirect handling during websocket upgrade

## Examples

- Component examples: <https://github.com/espressif/esp-protocols/tree/master/components/esp_websocket_client/examples>
- Feature showcase example (comprehensive walkthrough): <https://github.com/espressif/esp-protocols/tree/master/examples/websocket_features>

## Documentation

- Full HTML documentation: <https://docs.espressif.com/projects/esp-protocols/esp_websocket_client/docs/latest/index.html>
