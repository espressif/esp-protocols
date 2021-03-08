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

### DCE

This is the basic operational unit of the esp_modem component, abstracting a specific module in software,
which is basically configured by 
* the I/O communication media (UART), defined by the DTE configuration
* the specific command library supported by the device model, defined with the module type
* network interface configuration (PPPoS config in lwip)

After the object is created, the application interaction with the DCE is in
* issuing specific commands to the modem
* switching between data and command mode
