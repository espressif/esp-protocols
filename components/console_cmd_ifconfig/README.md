# Console command ifconfig
The component offers a console that enables runtime network interface configuration and monitoring for any example project.

## API

### Steps to enable console in an example code:
1. Add this component to your project using the command:
    ```bash
    idf.py add-dependency
    ```
2. In the main file of the example, add the following line:
    ```c
    #include "console_connect.h"
    ```
3. Ensure esp-netif is initialized and default event loop is created in your app_main():
    ```c
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ```
4. In your app_main() function, add the following line as the last line:
    ```c
    example_start_networking_console(NULL, NULL);
    ```
5. Optionally, you can add a user-defined command:
    ```c
    example_start_networking_console("user_cmd", usr_cmd_hndl);
    ```
    In the above line, "user_cmd" is a string representing the user-defined command name, and usr_cmd_hndl is the command callback function with the prototype.
    ```c
    int usr_cmd_hndl(int argc, char **argv)
    ```


## Suported commands:

### Ifconfig:
* **ifconfig help:** Prints the help text for all ifconfig commands
* **ifconfig netif create/destroy \<ethernet handle id\>/\<iface\>:** Create or destroy a network interface with the specified ethernet handle or interface name
* **ifconfig eth show:** Display a list of available ethernet handle
* **ifconfig:** Display a list of all esp_netif interfaces along with their information.
* **ifconfig \<iface>:** Provide the details of the named interface.
* **ifconfig \<iface> default:** Set the specified interface as the default interface.
* **ifconfig \<iface> ip6:** Enable IPv6 on the specified interface.
* **ifconfig <iface> up:** Enable the specified interface.
* **ifconfig <iface> down:** Disable the specified interface.
* **ifconfig \<iface> link \<up/down>:** Enable or disable the link of the specified interface.
* **ifconfig \<iface> ip \<ipv4 address>:** Set the IPv4 address of the specified interface.
* **ifconfig \<iface> mask \<ipv4 address>:** Set the subnet mask of the specified interface.
* **ifconfig \<iface> gw \<ipv4 address>:** Set the default gateway of the specified interface.
* **ifconfig \<iface> napt \<enable/disable>:** Enable or disable Network Address and Port Translation (NAPT) on the specified interface.
* **ifconfig \<iface> dhcp server \<enable/disable>:** Enable or disable the DHCP server on the specified interface. (Note: DHCP server is not supported yet)
* **ifconfig \<iface> dhcp client \<enable/disable>:** Enable or disable the DHCP client on the specified interface.

Note: Disabling the DHCP server and client enables the use of static IP configuration.

### Quit:
**quit:** Quits the Console application.
