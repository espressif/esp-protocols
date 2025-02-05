# ESP32 libwebsockets Port

This is a lightweight port of the libwebsockets library designed to run on the ESP32. It provides WebSocket client functionalities.

## Supported Options

The ESP32 port of libwebsockets supports a set of common WebSocket configurations for clients. These options can be configured through a structure passed to the `lws_create_context()` function.

Key features supported:
- WebSocket with optional SSL/TLS
- HTTP/1.1 client support

## Memory Footprint Considerations

The memory consumption primarily depends on the number of concurrent connections and the selected options for WebSocket frames, protocol handling, and SSL/TLS features. It consumes approximately 300 kB of program memory.

### Client:
- **Initial Memory Usage**: ~8 kB of heap on startup
- **Connected Memory Usage**: ~31 kB of heap after connected to a server

- When configuring a WebSocket client, ensure that you have enough heap space to handle the desired number of concurrent client connections.
- SSL/TLS configurations may require additional memory overhead, depending on the certificate size and cryptographic settings.
