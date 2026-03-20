# ESP PPP Link component (eppp_link)

The component provides a general purpose connectivity engine between two microcontrollers, one acting as PPP server, the other one as PPP client.
This component could be used for extending network using physical serial connection. Applications could vary from providing RPC engine for multiprocessor solutions to serial connection to POSIX machine. This uses a standard PPP protocol (if enabled) to negotiate IP addresses and networking, so standard PPP toolset could be used, e.g. a `pppd` service on linux. Typical application is a WiFi connectivity provider for chips that do not have WiFi.
Uses simplified TUN network interface by default to enable faster data transfer on non-UART transports.

## Typical application

Using this component we can construct a WiFi connectivity gateway on PPP channel. The below diagram depicts an application where
PPP server is running on a WiFi capable chip with NAPT module translating packets between WiFi and PPPoS interface.
We usually call this node a communication coprocessor, or a "SLAVE" microcontroller.
The main microcontroller (sometimes also called the "HOST") runs PPP client and connects only to the serial line,
brings in the WiFi connectivity from the communication coprocessor.

```
        Communication coprocessor                        Main microcontroller
    \|/  +----------------+                               +----------------+
     |   |                |       (serial) line           |                |
     +---+ WiFi NAT PPPoS |=== UART / SPI / SDIO / ETH ===| PPPoS client   |
         |        (server)|                               |                |
         +----------------+                               +----------------+
```

## Features

### Network Interface Modes

Standard PPP Mode (where PPP protocols is preferred) or simple tunnel using TUN Mode.

### Transport layer

UART, SPI, SDIO, Ethernet

### Support for logical channels

Allows channeling custom data (e.g. 802.11 frames)

## (Other) usecases

Besides the communication coprocessor example mentioned above, this component could be used to:
* Bring Wi-Fi connectivity to a computer using ESP32 chip.
* Connect your microcontroller to the internet via a pppd server (running on a raspberry)
* Bridging two networks with two microcontrollers

## Configuration

### Choose the transport layer

Use `idf.py menuconfig` to select the transport layer:

* `CONFIG_EPPP_LINK_UART` -- Use UART transport layer
* `CONFIG_EPPP_LINK_SPI` -- Use SPI transport layer
* `CONFIG_EPPP_LINK_SDIO` -- Use SDIO transport layer
* `CONFIG_EPPP_LINK_ETHERNET` -- Use Ethernet transport
  - Note: Ethernet creates it's own task, so calling `eppp_perform()` would not work
  - Note: Add dependency to ethernet_init component to use other Ethernet drivers
  - Note: You can override functions `eppp_transport_ethernet_deinit()` and `eppp_transport_ethernet_init()` to use your own Ethernet driver

### Choose the network interface

Use PPP netif for UART; Keep the default (TUN) for others

### Channel support (multiple logical channels)

* `CONFIG_EPPP_LINK_CHANNELS_SUPPORT` -- Enable support for multiple logical channels (default: disabled)
* `CONFIG_EPPP_LINK_NR_OF_CHANNELS` -- Number of logical channels (default: 2, range: 1-8, only visible if channel support is enabled)

When channel support is enabled, the EPPP link can multiplex multiple logical data streams over the same transport. The number of channels is configurable. Channel support is not available for Ethernet transport.

To use channels in your application, use the `eppp_add_channels()` API and provide your own channel transmit/receive callbacks. These APIs and related types are only available when channel support is enabled in Kconfig.

## API

### Client

* `eppp_connect()` -- Simplified API. Provides the initialization, starts the task and blocks until we're connected

### Server

* `eppp_listen()` -- Simplified API. Provides the initialization, starts the task and blocks until the client connects

### Manual actions

* `eppp_init()` -- Initializes one endpoint (client/server).
* `eppp_deinit()` -- Destroys the endpoint
* `eppp_netif_start()` -- Starts the network, could be called after startup or whenever a connection is lost
* `eppp_netif_stop()` --  Stops the network
* `eppp_perform()` -- Perform one iteration of the PPP task (need to be called regularly in task-less configuration)
#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
* `eppp_add_channels()` -- Register channel transmit/receive callbacks (only available if channel support is enabled)
#endif

## Throughput

Tested with WiFi-NAPT example

### UART @ 3Mbauds

* TCP - 2Mbits/s
* UDP - 2Mbits/s

### SPI @ 16MHz

* TCP - 5Mbits/s
* UDP - 8Mbits/s

### SDIO

* TCP - 9Mbits/s
* UDP - 11Mbits/s

### Ethernet

- Internal EMAC with real PHY chip
    * TCP - 5Mbits/s
    * UDP - 8Mbits/s
