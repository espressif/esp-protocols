# Modem TCP client

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## Overview
This example demonstrates how to act as a MQTT client using modem's TCP commands (provided, the device supports "socket" related commands)

This example could be used in two different configurations:
1) Custom TCP transport: Implements a TCP transport in form of AT commands and uses it as custom transport for mqtt client.
2) Localhost listener: Uses standard transports to connect and forwards socket layer data from the client to the modem using AT commands.

### Supported IDF versions

This example is supported from IDF `v5.0`.
