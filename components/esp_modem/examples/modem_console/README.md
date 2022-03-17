# Modem console example

## Overview

This example is mainly targets experimenting with a modem device, sending custom commands and switching to PPP mode using esp-console, command line API.
Please check the list of supported commands using `help` command.

This example implements two very simple network commands to demonstrate and test basic network functionality.
* `httpget`: Connect and get http content
* `ping`: Send ICMP pings

To demonstrate creating custom modem devices, this example creates a DCE object using a locally defined create method,
that sets up the DCE based on a custom module implemented in the `my_module_dce.hpp` file. The module class only overrides
`get_module_name()` method supplying a user defined name, but keeps all other commands the same as defined in the `GenericModule`
class.

### USB DTE support

For USB enabled targets (ESP32-S2 and ESP32-S3), it is possible to connect to the modem device via USB.
1. In menuconfig, navigate to `Example Configuration->Type of serial connection to the modem` and choose `USB`.
2. Connect the modem USB signals to pin 19 (DATA-) and 20 (DATA+) on your ESP chip.

USB example uses Quactel BG96 modem device.

### Supported IDF versions

This example is only supported from `v4.2`, due to support of the console repl mode.

USB example is supported from `v4.4`.