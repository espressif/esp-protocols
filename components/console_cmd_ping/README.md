# Console command ping and DNS server configuration
The component provides a console where the 'ping' command, 'getaddrinfo', and DNS server configuration commands can be executed.

## API

### Steps to enable console in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. In the main file of the example, add the following line:
    ```c
    #include "console_ping.h"
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

    // To register only ping/dns command skip calling console_cmd_all_register()
    ESP_ERROR_CHECK(console_cmd_ping_register());
    ESP_ERROR_CHECK(console_cmd_getaddrinfo_register());
    ESP_ERROR_CHECK(console_cmd_setdnsserver_register());
    ESP_ERROR_CHECK(console_cmd_getdnsserver_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

### Adding a plugin command or component:
To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

Note: **Auto-registration** of a specific plugin command can be disabled from menuconfig.

For more details refer [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)


## Suported command:

### ping:
```
ping  [-W <t>] [-i <t>] [-s <n>] [-c <n>] [-Q <n>] [-T <n>] [-I <n>] <host>
  send ICMP ECHO_REQUEST to network hosts
  -W, --timeout=<t>  Time to wait for a response, in seconds
  -i, --interval=<t>  Wait interval seconds between sending each packet
  -s, --size=<n>  Specify the number of data bytes to be sent
  -c, --count=<n>  Stop after sending count packets
  -Q, --tos=<n>  Set Type of Service related bits in IP datagrams
  -T, --ttl=<n>  Set Time to Live related bits in IP datagrams
  -I, --interface=<n>  Set Interface number 0=no-interface selected, >0 netif number + 1 (1 is usually 'lo0')
        <host>  Host address

getaddrinfo  [-f <AF>] [-F <FLAGS>]... [-p <port>] <hostname>
  Usage: getaddrinfo [options] <hostname> [service]
  -f, --family=<AF>  Address family (AF_INET, AF_INET6, AF_UNSPEC).
  -F, --flags=<FLAGS>  Special flags (AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST, AI_V4MAPPED, AI_ALL).
  -p, --port=<port>  String containing a numeric port number.
    <hostname>  Host address

setdnsserver  <main> [backup] [fallback]
  Usage: setdnsserver <main> [backup] [fallback]
        <main>  The main DNS server IP address.
        backup  The secondary DNS server IP address (optional).
      fallback  The fallback DNS server IP address (optional).

getdnsserver
  Usage: getdnsserver
```
These commands allow you to configure and retrieve DNS server settings on your ESP32 device, in addition to the existing ping functionality.

## Usage
### Using the setdnsserver command:
1. To set the main DNS server:
```
setdnsserver 8.8.8.8
```

2. To set the main and backup DNS servers:

```
setdnsserver 8.8.8.8 fe80::b0be:83ff:fe77:dd64
```

3. To set the main, backup, and fallback DNS servers:

```
setdnsserver 8.8.8.8 fe80::b0be:83ff:fe77:dd64 www.xyz.com
```

### Using the getdnsserver command:
To get the current DNS server settings:
```
getdnsserver
```

### Using the getaddrinfo command:
1. To get address information for a hostname:

```
getaddrinfo www.example.com
```

2. To specify additional options:

```
getaddrinfo -f AF_INET -F AI_PASSIVE www.example.com
```

### Using the ping command:
1. To ping a host:

```
ping www.example.com
```

2. To specify additional options, such as timeout, interval, packet size, interface, etc.:

```
ping -W 5 -i 1 -s 64 -c 4 -Q 0x10 -T 64 -I 0 www.example.com
```
