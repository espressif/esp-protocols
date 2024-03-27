# MQTT demo application that runs on linux

## Overview

This is a simple example demonstrating connecting to an MQTT broker, subscribing and publishing some data.
This example uses IDF build system and could be configured to be build and executed:
* for any ESP32 family chip
* for linux target

## How to use example

### Hardware Required

To run this example, you need any ESP32 development board or just PC/virtual machine/container running linux operating system.

### Host build modes

Linux build is supported in these two modes:
* `WITH_LWIP=0`: Without lwIP component. The project uses linux BSD socket interface to interact with TCP/IP stack. There's no connection phase, we use the host network as users. This mode is often referred to as user-side networking.
* `WITH_LWIP=1`: Including lwIP component, which is added to the list of required components and compiled on host. In this mode, we have to map the host network (linux TCP/IP stack) to the target network (lwip). We use IDF's [`tapif_io`](https://github.com/espressif/esp-idf/tree/master/examples/common_components/protocol_examples_tapif_io) component to create a network interface, which will be used to pass packets to and from the simulated target. Please refer to the [README](https://github.com/espressif/esp-idf/tree/master/examples/common_components/protocol_examples_tapif_io#readme) for more details about the host side networking.

### Building on linux

1) Configure linux target
```bash
idf.py --preview set-target linux
```

2) Build the project with preferred components (with or without lwip)
```bash
idf.py -DWITH_LWIP=0 build  # Building without lwip (user networking)
idf.py -DWITH_LWIP=1 build  # Building with lwip (TAP networking)
```

3) Run the project

It is possible to run the project elf file directly, or using `idf.py` monitor target (no need to flash):
```bash
idf.py monitor
```
idf.py -DWITH_LWIP=0 build  # Building without lwip (user networking)
