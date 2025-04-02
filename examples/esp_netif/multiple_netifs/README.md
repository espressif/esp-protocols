| Supported Targets | ESP32 |
| ----------------- | ----- |

# Multiple Interface example

## Overview

This example demonstrates working with multiple different interfaces with different priorities. It creates these interfaces and tries to connect:
* WiFi Station
* Ethernet using ESP32 internal ethernet driver
* PPPoS over cellular modem

## How to use example

* Set the priorities and the host name for the example to ICMP ping.
* The example will initialize all interfaces
* The example will start looping and checking connectivity to the host name
  * It prints the default interface and ping output
  * It tries to reconfigure DNS server if host name resolution fails
  * It tries to manually change the default interface if connection fails

### Handling DNS server across interfaces

This example also demonstrates how DNS servers could be handled on network interface level, as lwIP used global DNS server information.

All network interfaces store their DNS info upon acquiring an IP in the internal structure (in the application code) and the DNS servers are restored if host name resolution fails.

This functionality is handled in IDF (supported from v5.3) automatically if `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF` is enabled, the DNS server info keeps updating per network interface in IDF layers. This examples uses the IDF functionality if `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF=1`.

### Hardware Required

To run this example, it's recommended that you have an official ESP32 Ethernet development board - [ESP32-Ethernet-Kit](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/get-started-ethernet-kit.html).
You would also need a modem connected to the board using UART interface.
