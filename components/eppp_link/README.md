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

* `eppp_client_init()` -- Initialization of the client. Need to run only once.
* `eppp_client_start()` -- Starts the connection, could be called after startup or whenever a connection is lost
* `eppp_client_perform()` -- Perform one iteration of the PPP task (need to be called regularly in task-less configuration)

### Server

* `eppp_listen()` -- Simplified API. Provides the initialization, starts the task and blocks until the client connects
* `eppp_server_init()` -- Initialization of the server. Need to run only once.
* `eppp_server_start()` -- (Re)starts the connection, should be called after startup or whenever a connection is lost
* `eppp_server_perform()` -- Perform one iteration of the PPP task (need to be called regularly in task-less configuration)

## Throughput

Tested with WiFi-NAPT example, no IRAM optimizations

### UART @ 3Mbauds

* TCP - 2Mbits
* UDP - 2Mbits

### SPI @ 40MHz

* TCP - 6Mbits
* UDP - 10Mbits
