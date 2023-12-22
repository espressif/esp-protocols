# Console command wifi
The component offers a console with a command that enables runtime wifi configuration for any example project.

## API

### Steps to enable console in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. In the main file of the example, add the following line:
    ```c
    #include "console_wifi.h"
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

    // To register only wifi command skip calling console_cmd_all_register()
    ESP_ERROR_CHECK(console_cmd_wifi_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

## Suported command:

### wifi:
* ```wifi help```: Prints the help text for all wifi commands
* ```wifi show network```: Scans and displays upto 10 available wifi networks.
* ```wifi show sta```: Shows the details of wifi station.
* ```wifi sta join <network ssid> <password>```: Station joins the given wifi network.
* ```wifi sta join <network ssid>```: Station joins the given unsecured wifi network.
* ```wifi sta leave```: Station leaves the wifi network.
