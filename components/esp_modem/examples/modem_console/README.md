# PPPoS simple client example

(See the README.md file in the upper level 'examples' directory for more information about examples.)

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

### Supported IDF versions

This example is only supported from `v4.2`, due to support of the console repl mode. 