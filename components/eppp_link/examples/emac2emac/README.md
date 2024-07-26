
# EPPP link with EMAC to EMAC transport layer

This example runs a symmetrical server-client eppp application with iperf component using Ethernet transport layer, with a customized Ethernet driver from [eth_dummy_phy](https://components.espressif.com/components/espressif/eth_dummy_phy) component.

Note, that when using Ethernet with Wi-Fi on ESP32, you shouldn't use RMII CLK output, for more details, please read:
```
 idf.py docs -sp api-reference/network/esp_eth.html#_CPPv422emac_rmii_clock_gpio_t
```

Please refer to the component documentation for more information about the principle of operation and the actual physical connection between nodes.

## How to use this example

* Choose `CONFIG_EXAMPLE_NODE_SERVER` for the device connected as **RMII CLK Source Device**
* Choose `EXAMPLE_NODE_CLIENT` for the device connected as **RMII CLK Sink Device**
* Run `iperf` command on both sides to check the network performance (both server and client iperf role could be used on both devices)
