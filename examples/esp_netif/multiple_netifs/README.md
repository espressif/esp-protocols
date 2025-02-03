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

This example demonstrate how DNS servers could be handled on network interface level, as lwIP used global DNS server information. All network interfaces store the DNS info upon acquiring an IP in the internal structure and the DNS servers are restored if host name resolution fails.
If `CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF` is supported and enabled, the DNS server update per network interface is handled automatically in IDF.

### Hardware Required

To run this example, it's recommended that you have an official ESP32 Ethernet development board - [ESP32-Ethernet-Kit](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/get-started-ethernet-kit.html).
You would also need a modem connected to the board using UART interface.
