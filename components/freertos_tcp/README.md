# FreeRTOS-Plus-TCP for ESP-IDF

ESP-IDF port of Amazon FreeRTOS-Plus-TCP - a lightweight TCP/IP stack providing an alternative to lwIP.

## MVP Status

**This is a Minimum Viable Product (MVP) release focusing on IPv4-only, single network interface functionality.**

**What's Included in MVP:**
- ✅ Full TCP/UDP support with IPv4
- ✅ DHCP client with hostname registration
- ✅ DNS resolution with caching
- ✅ ARP with clash detection
- ✅ ICMP (ping) support
- ✅ mDNS, LLMNR, NBNS protocols
- ✅ Single WiFi station interface
- ✅ Basic test coverage

**Not Included in MVP (Future Releases):**
- ⚠️ IPv6 support (code present but disabled by default)
- ⚠️ DHCPv6
- ⚠️ Multiple network interfaces
- ⚠️ Ethernet interface testing
- ⚠️ Advanced examples

## Overview

This component integrates [FreeRTOS-Plus-TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP) as a custom TCP/IP stack for ESP-IDF. It provides a Berkeley sockets-like interface with fully scalable features and RAM footprint, making it suitable for various ESP32 applications.

**Version**: 0.1.0
**Minimum ESP-IDF**: 5.0

## Architecture

The component consists of three main layers:

### 1. FreeRTOS-Plus-TCP Core (`FreeRTOS-Plus-TCP/`)
- Upstream library (Git submodule)
- Complete TCP/IP stack implementation
- Provides: TCP, UDP, IPv4, IPv6, DHCP, DNS, ARP, ICMP, mDNS, LLMNR, NBNS

### 2. ESP-IDF Port Layer (`port/`)
- **`NetworkInterface.c`**: Network interface driver integration with ESP-IDF
- **`FreeRTOSIPConfig.h`**: Stack configuration (maps Kconfig to FreeRTOS-Plus-TCP settings)
- **`FreeRTOSIPConfig.c`**: Additional port-specific configuration
- **`FreeRTOS_AppHooks.c`**: Application hook implementations

### 3. ESP-IDF Integration (`esp_netif/`)
- **`esp_netif_impl.c`**: Custom `esp_netif` implementation for FreeRTOS-Plus-TCP
- **`interface.c`/`interface.h`**: Bridge between `esp_netif` and FreeRTOS-Plus-TCP
- Enables use of standard ESP-IDF WiFi/Ethernet drivers with FreeRTOS-Plus-TCP

## Features

### Networking Protocols
- ✅ **TCP**: Full TCP implementation with windowing
- ✅ **UDP**: UDP support
- ✅ **IPv4**: Full IPv4 stack (MVP focus)
- ⚠️ **IPv6**: Experimental - disabled by default in MVP (`CONFIG_AFPT_IPV6=n`)
- ✅ **DHCP**: DHCPv4 client with hostname registration
- ❌ **DHCPv6**: Not included in MVP (`ipconfigUSE_DHCPv6 = 0`)
- ✅ **DNS**: DNS client with caching (6 addresses per entry)
- ✅ **ARP**: Address Resolution Protocol with clash detection
- ✅ **ICMP**: Ping support (both incoming and outgoing)
- ✅ **mDNS**: Multicast DNS
- ✅ **LLMNR**: Link-Local Multicast Name Resolution
- ✅ **NBNS**: NetBIOS Name Service

### Stack Features
- **Zero-copy**: TX/RX drivers for better performance
- **Socket API**: Berkeley sockets interface
- **Select support**: `FreeRTOS_select()` for multiplexing
- **TCP keep-alive**: Configurable (20s interval)
- **Network event hooks**: For monitoring network state changes
- **Custom ethernet frames**: Processing support

### Configuration
- Configurable via `menuconfig` under "Amazon FreeRTOS plus TCP (AFpT)"
- IPv4/IPv6 enable/disable options
- Custom hostname support
- Network parameters (MTU, buffer counts, timeouts, etc.)

## Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `AFPT_ENABLE` | Y | Enables FreeRTOS-Plus-TCP (auto-enabled) |
| `AFPT_LOCAL_HOSTNAME` | 'espressif' | Default device hostname |
| `AFPT_IPV4` | Y | Enable IPv4 stack (required for MVP) |
| `AFPT_IPV6` | **N** | Enable IPv6 stack (experimental, not in MVP) |

### Key Stack Parameters (in `FreeRTOSIPConfig.h`)
- **Network MTU**: 1500 bytes
- **Network buffers**: 60 descriptors
- **TCP RX buffer**: 10000 bytes per socket
- **TCP TX buffer**: 10000 bytes per socket
- **TCP window segments**: 240 (supports ~10 simultaneous sockets)
- **ARP cache entries**: 6
- **DNS cache entries**: Enabled with 6 addresses per entry
- **IP task priority**: `configMAX_PRIORITIES - 2`
- **IP task stack**: `configMINIMAL_STACK_SIZE * 5`

## Usage

### Basic Example

```c
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

// Create socket
Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET,
                                FREERTOS_SOCK_STREAM,
                                FREERTOS_IPPROTO_TCP);

// Resolve hostname
struct freertos_addrinfo hints = { .ai_family = FREERTOS_AF_INET };
struct freertos_addrinfo *results = NULL;
FreeRTOS_getaddrinfo("example.com", NULL, &hints, &results);

// Connect
struct freertos_sockaddr addr;
addr.sin_family = FREERTOS_AF_INET;
addr.sin_port = FreeRTOS_htons(80);
addr.sin_address.ulIP_IPv4 = results->ai_addr->sin_address.ulIP_IPv4;
FreeRTOS_connect(sock, &addr, sizeof(addr));

// Send/Receive
FreeRTOS_send(sock, data, len, 0);
FreeRTOS_recv(sock, buffer, buf_len, 0);

// Close
FreeRTOS_closesocket(sock);
```

See `examples/simple/` for a complete working example.

## Integration with ESP-IDF

The component implements ESP-IDF's `esp_netif` custom stack interface:
- Automatically selected via `ESP_NETIF_PROVIDE_CUSTOM_IMPLEMENTATION`
- Compatible with standard ESP-IDF WiFi and Ethernet drivers
- Uses ESP-IDF's event system for network events
- Supports standard `esp_netif` API for basic operations

### Multi-Core (SMP) Support

The component is **SMP-safe** and works correctly on dual-core ESP32 chips:

- ✅ **Thread-safe**: Uses ESP-IDF's `portMUX_TYPE` spinlocks for critical sections
- ✅ **Cross-core safe**: Socket APIs can be called from tasks on any core
- ✅ **ISR-safe**: Network buffer allocation protected in ISR context
- ✅ **Queue-based**: IP task communicates via SMP-safe FreeRTOS queues

**Important**: Individual sockets are **not thread-safe**. If multiple tasks share the same socket, you must provide your own synchronization (semaphores/mutexes).

**Note**: The IP task runs with no core affinity (can execute on either core). This is functionally safe but may be optimized in future releases.

## Building

### Component Dependencies
- **Required**: `esp_wifi`, `esp_netif`, `freertos` (from ESP-IDF)
- **IDF Version**: >= 5.0

### Project Setup

Add to your project's `idf_component.yml`:
```yaml
dependencies:
  freertos_tcp:
    version: ">=0.1.0"
    git: https://github.com/espressif/esp-protocols.git
    path: components/freertos_tcp
```

Or clone manually to your project's `components/` directory.

## Testing

The component includes a comprehensive test application in `test_app/` that validates:
- TCP client connections
- UDP send/receive
- DNS resolution and caching
- DHCP IP acquisition
- Socket options

See [TESTING.md](TESTING.md) for detailed information on running tests.

## Examples

### Simple TCP Client (`examples/simple/`)
Basic TCP client demonstrating:
- Initializing network interface
- DNS resolution
- TCP connection
- Sending HTTP request
- Receiving response

To build and run:
```bash
cd examples/simple
idf.py build flash monitor
```

## File Structure

```
freertos_tcp/
├── CMakeLists.txt              # Component build configuration
├── Kconfig                     # Configuration menu options
├── idf_component.yml           # Component metadata
├── README.md                   # This file
├── FreeRTOS-Plus-TCP/          # Upstream library (submodule)
│   └── source/                 # TCP/IP stack source files
├── port/                       # ESP-IDF port layer
│   ├── NetworkInterface.c      # Network interface driver
│   ├── FreeRTOSIPConfig.c      # Configuration implementation
│   ├── FreeRTOS_AppHooks.c     # Application hooks
│   └── include/
│       └── FreeRTOSIPConfig.h  # Stack configuration header
├── esp_netif/                  # ESP-IDF integration
│   ├── esp_netif_impl.c        # esp_netif implementation
│   ├── interface.c             # Interface bridge
│   └── interface.h             # Interface definitions
└── examples/
    └── simple/                 # Simple TCP client example
```

## Roadmap (Post-MVP)

Future releases may include:

### Next Release
- **IPv6 Support**: Enable and test IPv6 functionality
- **Multiple Interface Support**: WiFi AP + STA, Ethernet + WiFi
- **Additional Examples**: TCP server, UDP examples, mDNS service registration
- **Ethernet Testing**: Validate with ESP32 Ethernet drivers

### Future Enhancements
- **DHCPv6**: IPv6 address autoconfiguration
- **Performance Optimization**: Memory pool tuning, zero-copy improvements
- **Advanced Features**: IPv6 SLAAC, custom protocol handlers
- **Migration Guide**: Detailed lwIP to FreeRTOS-Plus-TCP migration documentation
- **Benchmarking**: Performance and memory footprint comparison with lwIP

### Upstream Tracking
- **FreeRTOS-Plus-TCP**: Git submodule (regularly updated upstream)
- Monitor upstream security advisories and bug fixes
- Consider version pinning strategy for stable releases

## Debugging

Enable debug output via:
```c
#define ipconfigHAS_DEBUG_PRINTF    1
```
in `FreeRTOSIPConfig.h` (already enabled by default).

Output uses standard `printf()` - configure via ESP-IDF's logging system.

## Resources

- [FreeRTOS-Plus-TCP Documentation](http://www.FreeRTOS.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/TCP_Networking_Tutorial.html)
- [FreeRTOS-Plus-TCP API Reference](https://freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/FreeRTOS_TCP_API_Functions.html)
- [FreeRTOS-Plus-TCP GitHub](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)

## License

- FreeRTOS-Plus-TCP core: MIT License (Amazon)
- ESP-IDF port layer: Apache-2.0 (Espressif Systems)
- See individual files for SPDX license identifiers

## Support

For issues specific to the ESP-IDF port, please use the [esp-protocols issue tracker](https://github.com/espressif/esp-protocols/issues).

For general FreeRTOS-Plus-TCP questions, see the [FreeRTOS Community Support Forum](https://forums.freertos.org).
