# Socket Utilities

This component provides a simplified implementation of various socket utilities using the `lwIP` and `esp_netif` components. It is particularly useful for porting Linux-based libraries where performance and memory constraints are not a primary concern.

## Supported Functions

| API            | Description                        | Limitations                                      |
|----------------|------------------------------------|--------------------------------------------------|
| `ifaddrs()`    | Uses `esp_netif` internally        | Only supports IPv4 addresses                     |
| `socketpair()` | Implements using two `lwIP` loopback stream sockets | Only supports IPv4 sockets      |
| `pipe()`       | Wraps `socketpair()`               | Utilizes bidirectional sockets instead of pipes  |
| `getnameinfo()`| Uses `lwIP`'s `inet_ntop()`        | Supports only IPv4, and only `NI_NUMERICHOST` and `NI_NUMERICSERV` flags |
| `gai_strerror()`| Simple `itoa`-like implementation | Only returns the numeric error code as a string   |
