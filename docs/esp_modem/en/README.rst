ESP MODEM
=========

This component is used to communicate with modems in the command mode
(using AT commands), as well as the data mode (over PPPoS protocol). The
modem device is modeled with a DCE (Data Communication Equipment)
object, which is composed of:

- DTE (Data Terminal Equipment), which abstracts the terminal (currently only UART implemented).
- PPP Netif representing a network interface communicating with the DTE using PPP protocol.
- Module abstracting the specific device model and its commands.

::

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

Modem components
----------------

DCE
~~~

This is the basic operational unit of the esp_modem component,
abstracting a specific module in software, which is basically configured
by

- the I/O communication media (UART), defined by the DTE configuration
- the specific command library supported by the device  model, defined with the module type
- network interface configuration (PPPoS config in lwip)

After the object is created, the application interaction with the DCE is
in

- issuing specific commands to the modem
- switching between data and command mode

DTE
~~~

Is an abstraction of the physical interface connected to the modem.
Current implementation supports only UART

PPP netif
~~~~~~~~~

Is used to attach the specific network interface to a network
communication protocol used by the modem. Currently implementation
supports only PPPoS protocol.

Module
~~~~~~

Abstraction of the specific modem device. Currently the component
supports SIM800, BG96, SIM7600.

Use cases
---------

Users interact with the esp-modem using the DCE’s interface, to
basically

- Switch between command and data mode to connect to the internet via cellular network.
- Send various commands to the device (e.g. send SMS)

The applications typically register handlers for network events to
receive notification on the network availability and IP address changes.

Common use cases of the esp-modem are also listed as the examples:

- ``examples/pppos_client`` – simple client which reads some module properties and switches to the data mode to connect to a public mqtt broker.
- ``examples/modem_console`` – is an example to exercise all possible module commands in a console application.
- ``examples/ap_to_pppos`` – this example focuses on the network
connectivity of the esp-modem and provides a WiFi AP that forwards
packets (and uses NAT) to and from the PPPoS connection.

Extensibility
-------------

CMUX
~~~~

Implementation of virtual terminals is an experimental feature, which
allows users to also issue commands in the data mode, after creating
multiple virtual terminals, designating some of them solely to data
mode, others solely to command mode.

DTE’s
~~~~~

Currently, we support only UART (and USB as a preview feature), but
modern modules support other communication interfaces, such as USB, SPI.

Other devices
~~~~~~~~~~~~~

Adding a new device is a must-have requirement for the esp-modem
component. Different modules support different commands, or some
commands might have a different implementation. Adding a new device
means to provide a new implementation as a class derived from
``GenericModule``, where we could add new commands or modify the
existing ones.
If you have to support a custom device with C-API, please refer to
the example ``examples/pppos_client`` and enable ``ESP_MODEM_ADD_CUSTOM_MODULE``.
For advanced use-case, mainly with C++ API and/or usage of esp_modem's
Factory class, please read <advanced_api>.

Configuration
-------------

Modem abstraction is configurable both compile-time and run-time.

Component Kconfig
~~~~~~~~~~~~~~~~~

Compile-time configuration is provided using menuconfig. Please check
the description for the CMUX mode configuration options.

Runtime configuration
~~~~~~~~~~~~~~~~~~~~~

Is defined using standard configuration structures for ``DTE`` and
``DCE`` objects separately. Please find documentation of

- :cpp:class:``esp_modem_dte_config_t``
- :cpp:class:``esp_modem_dce_config_t``

Known issues
------------

There are certain issues, especially in using CMUX mode which you can
experience with some devices:

1) Some modems (e.g. A76xx serries) use 2 bytes CMUX payload, which
might cause buffer overflow when trying to defragment the payload.
It's recommended to disable ``ESP_MODEM_CMUX_DEFRAGMENT_PAYLOAD``,
which will fix the issue, but may occasional cause reception of AT command
replies in fragments.

2) Some devices (such as SIM7000) do not support CMUX mode at all.

3) Device A7670 does no not correctly exit CMUX mode. You can apply
this patch to adapt the exit sequence https://github.com/espressif/esp-protocols/commit/28de34571012d36f2e87708955dcd435ee5eab70

::

      diff --git a/components/esp_modem/src/esp_modem_cmux.cpp b/components/esp_modem/src/esp_modem_cmux.cpp
      index 0c480f8..4418c3d 100644
      --- a/components/esp_modem/src/esp_modem_cmux.cpp
      +++ b/components/esp_modem/src/esp_modem_cmux.cpp
      @@ -206,6 +206,15 @@ bool CMux::on_header(CMuxFrame &frame)
      }
      size_t payload_offset = std::min(frame.len, 4 - frame_header_offset);
      memcpy(frame_header + frame_header_offset, frame.ptr, payload_offset);
      +    if (frame_header[1] == 0xEF) {
      +        dlci = 0;
      +        type = frame_header[1];
      +        payload_len = 0;
      +        data_available(&frame.ptr[0], payload_len); // Notify DISC
      +        frame.advance(payload_offset);
      +        state = cmux_state::FOOTER;
      +        return true;
      +    }
      if ((frame_header[3] & 1) == 0) {
            if (frame_header_offset + frame.len <= 4) {
                  frame_header_offset += frame.len;
