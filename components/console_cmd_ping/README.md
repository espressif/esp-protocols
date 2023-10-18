# Console command ping
The component provides a console where the 'ping' command can be executed.

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

    // To register only ifconfig command skip calling console_cmd_all_register()
    ESP_ERROR_CHECK(console_cmd_ping_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

### Adding a plugin command or component:
To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

For more details refer [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)


## Suported command:

### ping:
```
ping  [-W <t>] [-i <t>] [-s <n>] [-c <n>] [-Q <n>] [-T <n>] <host>
  send ICMP ECHO_REQUEST to network hosts
  -W, --timeout=<t>  Time to wait for a response, in seconds
  -i, --interval=<t>  Wait interval seconds between sending each packet
  -s, --size=<n>  Specify the number of data bytes to be sent
  -c, --count=<n>  Stop after sending count packets
  -Q, --tos=<n>  Set Type of Service related bits in IP datagrams
  -T, --ttl=<n>  Set Time to Live related bits in IP datagrams
        <host>  Host address
```
