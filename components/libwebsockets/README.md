# ESP32 libwebsockets Port

This is a lightweight port of the libwebsockets library designed to run on the ESP32. It provides WebSocket client and server functionalities.

## Supported Options

The ESP32 port of libwebsockets supports a set of common WebSocket configurations, including both server and client modes. These options can be configured through a structure passed to the `lws_create_context()` function.

Key features supported:
- WebSocket server with optional SSL/TLS
- WebSocket client support
- HTTP/1.1 server and client support

## Mandatory Use of mbedTLS

For secure WebSocket communication (WSS) and SSL/TLS support, **mbedTLS** is mandatory. mbedTLS provides the cryptographic functions required for establishing secure connections and is included as part of the ESP32 toolchain.

- **mbedTLS** is required for all encrypted WebSocket (WSS) and HTTPS connections.
- Ensure that mbedTLS is properly configured in your ESP32 project, as it is essential for enabling SSL/TLS functionality.

## Memory Footprint Considerations

libwebsockets on the ESP32 has been optimized for minimal memory usage. The memory consumption primarily depends on the number of concurrent connections and the selected options for WebSocket frames, protocol handling, and SSL/TLS features.

- **Initial Memory Usage**: TBD
- **Per Client Memory Usage**: TBD
- **Per Server Memory Usage**: TBD

### Memory Management Tips:
- When configuring a WebSocket server, ensure that you have enough heap space to handle the desired number of concurrent client connections.
- SSL/TLS configurations may require additional memory overhead, depending on the certificate size and cryptographic settings.

## Testing

TBD
