# ESP MODEM

This component is used to communicate with modems in the command mode (using AT commands), as well as the data mode
(over PPPoS protocol).
The modem device is modeled with a DCE (Data Communication Equipment) object, which is composed of:
* DTE (Data Terminal Equipment), which abstracts the terminal (currently only UART implemented).
* PPP Netif representing a network interface communicating with the DTE using PPP protocol.
* Module abstracting the specific device model and its commands.

```
   +-----+
   | DTE |--+
   +-----+  |   +-------+
            +-->|   DCE |
   +-------+    |       |o--- set_mode(command/data)
   | Module|--->|       |
   +-------+    |       |o--- send_commands
             +->|       |
   +------+  |  +-------+
   | PPP  |--+
   | netif|------------------> network events
   +------+
```

## Modem components
### DCE

This is the basic operational unit of the esp_modem component, abstracting a specific module in software,
which is basically configured by
* the I/O communication media (UART), defined by the DTE configuration
* the specific command library supported by the device model, defined with the module type
* network interface configuration (PPPoS config in lwip)

After the object is created, the application interaction with the DCE is in
* issuing specific commands to the modem
* switching between data and command mode

### DTE
Is an abstraction of the physical interface connected to the modem. Current implementation supports only UART

### PPP netif

Is used to attach the specific network interface to a network communication protocol used by the modem. Currently implementation supports only PPPoS protocol.

### Module

Abstraction of the specific modem device. Currently the component supports SIM800, BG96, SIM7600.

## Use cases

Users interact with the esp-modem using the DCE's interface, to basically
* Switch between command and data mode to connect to the internet via cellular network.
* Send various commands to the device (e.g. send SMS)

The applications typically register handlers for network events to receive notification on the network availability and
IP address changes.

Common use cases of the esp-modem are also listed as the examples:
* `examples/pppos_client` -- simple client which reads some module properties and switches to the data mode to connect to a public mqtt broker.
* `examples/modem_console` -- is an example to exercise all possible module commands in a console application.
* `examples/ap_to_pppos` -- this example focuses on the network connectivity of the esp-modem and provides a WiFi AP that forwards packets (and uses NAT) to and from the PPPoS connection.

## Extensibility

### CMUX

Implementation of virtual terminals is an experimental feature, which allows users to also issue commands in the data mode,
after creating multiple virtual terminals, designating some of them solely to data mode, others solely to command mode.

### DTE's

Currently, we support only UART (and USB as a preview feature), but modern modules support other communication interfaces, such as USB, SPI.

### Other devices

Adding a new device is a must-have requirement for the esp-modem component. Different modules support different commands,
or some commands might have a different implementation. Adding a new device means to provide a new implementation
as a class derived from `GenericModule`, where we could add new commands or modify the existing ones.

## Configuration

Modem abstraction is configurable both compile-time and run-time.

### Component Kconfig

Compile-time configuration is provided using menuconfig. Please check the description for the CMUX mode configuration options.

### Runtime configuration

Is defined using standard configuration structures for `DTE` and `DCE` objects separately. Please find documentation of
* :cpp:class:`esp_modem_dte_config_t`
* :cpp:class:`esp_modem_dce_config_t`
