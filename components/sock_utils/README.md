# Socket Utilities

This component provides simplified implementations of common socket-related utilities using `lwIP` and `esp_netif`. It is especially useful for porting Linux-based libraries to ESP32 where performance and memory constraints are secondary considerations.


## Supported Functions


| API              | Description                                                 | Limitations                                                       |
|------------------|-------------------------------------------------------------|-------------------------------------------------------------------|
| `ifaddrs()`      | Retrieves interface addresses using `esp_netif`             | IPv4 addresses only                                               |
| `socketpair()`   | Creates a pair of connected sockets using `lwIP` loopback stream sockets | IPv4 sockets only                                    |
| `pipe()`         | Wraps `socketpair()` to provide unidirectional pipe-like functionality | Uses bidirectional sockets in place of true pipes  |
| `getnameinfo()`  | Converts IP addresses to human-readable form using `lwIP`'s `inet_ntop()` | IPv4 only; supports `NI_NUMERICHOST` and `NI_NUMERICSERV` flags only |
| `gai_strerror()` | Returns error code as a string                              | Simple numeric string representation only                         |

**Note**: `socketpair()` and `pipe()` are built on top of `lwIP` TCP sockets, inheriting the same characteristics. For instance, the maximum transmit buffer size is based on the `TCP_SND_BUF` setting.
