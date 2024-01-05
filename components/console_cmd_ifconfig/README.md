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

    // Register all plugin command added to your project
    ESP_ERROR_CHECK(console_cmd_all_register());

    // To register only ifconfig command skip calling console_cmd_all_register()
    ESP_ERROR_CHECK(console_cmd_ifconfig_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

### Adding a plugin command or component:
To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

For more details refer [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)


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

## Usage:

### Creating an ethernet interface
```
esp> ifconfig eth init
Internal(IP101): pins: 23,18, Id: 0
esp> ifconfig netif create 0
```

### Removing an interface and deinitializing ethernet
```
esp> ifconfig netif destroy en1
I (8351266) ethernet_init: Ethernet(IP101[23,18]) Link Down
I (8351266) ethernet_init: Ethernet(IP101[23,18]) Stopped
esp> ifconfig eth deinit
```

### Set default interface
```
esp> ifconfig en1 default
```

### Enable NAPT on an interface
```
esp> ifconfig en1 napt enable
I (8467116) console_ifconfig: Setting napt enable on en1
```

### Enable DHCP client on an interface
```
esp> ifconfig en1 dhcp client enable
```

### Enable static IP on an interface
```
esp> ifconfig en1 dhcp client disable
```

### Set static IP address
```
esp> ifconfig en1 ip 192.168.5.2
I (111466) console_ifconfig: Setting ip: 192.168.5.2
esp> ifconfig en1 mask 255.255.255.0
I (130946) console_ifconfig: Setting mask: 255.255.255.0
esp> ifconfig en1 gw 192.168.5.1
I (143576) console_ifconfig: Setting gw: 192.168.5.1
I (143576) esp_netif_handlers: eth ip: 192.168.5.2, mask: 255.255.255.0, gw: 192.168.5.1
```
