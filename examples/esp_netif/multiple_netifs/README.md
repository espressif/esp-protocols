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

### Hardware Required

To run this example, it's recommended that you have an official ESP32 Ethernet development board - [ESP32-Ethernet-Kit](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/get-started-ethernet-kit.html).
You would also need a modem connected to the board using UART interface.
