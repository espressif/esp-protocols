# ESP PPP Link component (eppp_link)

The component provides a general purpose connectivity engine between two micro-controllers, one acting as PPP server (slave), the other one as PPP client (host). Typical application is a WiFi connectivity provider for chips that do not have WiFi:

```
             SLAVE micro                                  HOST micro
    \|/  +----------------+                           +----------------+
     |   |                |          serial line      |                |
     +---+ WiFi NAT PPPoS |======== UART / SPI =======| PPPoS client   |
         |        (server)|                           |                |
         +----------------+                           +----------------+
```

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

## Throughput

Tested with WiFi-NAPT example, no IRAM optimizations

### UART @ 3Mbauds

* TCP - 2Mbits/s
* UDP - 2Mbits/s

### SPI @ 20MHz

* TCP - 6Mbits/s
* UDP - 10Mbits/s
