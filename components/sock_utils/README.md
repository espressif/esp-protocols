# Socket Utilities

This component provides simplified implementations of common socket-related utilities using `lwIP` and `esp_netif`. It is especially useful for porting Linux-based libraries to ESP32 where performance and memory constraints are secondary considerations.


## Supported Functions


| API                | Description                                                 | Limitations                                                       | Declared in                            |
|--------------------|-------------------------------------------------------------|-------------------------------------------------------------------|----------------------------------------|
| `ifaddrs()`        | Retrieves interface addresses using `esp_netif`             | IPv4 addresses only                                               | `ifaddrs.h`                            |
| `socketpair()` *)  | Creates a pair of connected sockets using `lwIP` loopback stream sockets | IPv4 sockets only                                    | `socketpair.h`, `sys/socket.h` **)     |
| `pipe()`       *) | Wraps `socketpair()` to provide unidirectional pipe-like functionality | Uses bidirectional sockets in place of true pipes  | `socketpair.h`, `unistd.h` ***)        |
| `getnameinfo()`    | Converts IP addresses to human-readable form using `lwIP`'s `inet_ntop()` | IPv4 only; supports `NI_NUMERICHOST` and `NI_NUMERICSERV` flags only | `getnameinfo.h`, `netdb.h` in ESP-IDF  |
| `gai_strerror()`   | Returns error code as a string                              | Simple numeric string representation only                         | `gai_strerror.h`, `netdb.h` **)        |
| `gethostname()`    | Returns lwip netif hostname                                 | Not a system-wide hostname, but interface specific hostname       | `gethostname.h`, `unistd.h` in ESP-IDF |

**Notes**:

- **`*)`** `socketpair()` and `pipe()` are built on top of `lwIP` TCP sockets, inheriting the same characteristics. For instance, the maximum transmit buffer size is based on the `TCP_SND_BUF` setting.
- **`**)`** `socketpair()` and `gai_strerror()` are declared in sock_utils header files, the declaration is propagated to ESP-IDF from v5.5 to the official header files. If you're using older IDF version, you need to manually pre-include related header files from the sock_utils public include directory.
- **`***)`** `pipe()` is declared in compiler's `sys/unistd.h`.
