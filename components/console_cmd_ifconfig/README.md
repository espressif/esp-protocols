# Console command ifconfig
The component offers a console with a command that enables runtime network interface configuration and monitoring for any example project.

## API

### Steps to enable console in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. In the main file of the example, add the following line:
    ```c
    #include "console_ifconfig.h"
    ```
3. Ensure esp-netif and NVS flash is initialized and default event loop is created in your app_main():
    ```c
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init();   //Initialize NVS
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ```
4. In your app_main() function, add the following line as the last line:
    ```c
    ESP_ERROR_CHECK(console_cmd_init());     // Initialize console
    ESP_ERROR_CHECK(console_cmd_ifconfig_register());
    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```


## Suported command:

### Ifconfig:
```
 ifconfig help: Prints the help text for all ifconfig commands
 ifconfig netif create/destroy <ethernet handle id>/<iface>: Create or destroy a network interface with the specified ethernet handle or interface name
 ifconfig eth init/deinit/show: Initialize, deinitialize and display a list of available ethernet handle
 ifconfig: Display a list of all esp_netif interfaces along with their information
 ifconfig <iface>: Provide the details of the named interface
 ifconfig <iface> default: Set the specified interface as the default interface
 ifconfig <iface> ip6: Enable IPv6 on the specified interface
 ifconfig <iface> up: Enable the specified interface
 ifconfig <iface> down: Disable the specified interface
 ifconfig <iface> link <up/down>: Enable or disable the link of the specified interface
 ifconfig <iface> napt <enable/disable>: Enable or disable NAPT on the specified interface.
 ifconfig <iface> ip <ipv4 addr>: Set the IPv4 address of the specified interface
 ifconfig <iface> mask <ipv4 addr>: Set the subnet mask of the specified interface
 ifconfig <iface> gw <ipv4 addr>: Set the default gateway of the specified interface
 ifconfig <iface> staticip: Enables static ip
 ifconfig <iface> dhcp server <enable/disable>: Enable or disable the DHCP server.(Note: DHCP server is not supported yet)
 ifconfig <iface> dhcp client <enable/disable>: Enable or disable the DHCP client.
Note: Disabling the DHCP server and client enables the use of static IP configuration.
```
