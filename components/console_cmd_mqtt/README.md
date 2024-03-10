# Console command mqtt
The component provides a console where mqtt commands can be executed.


## MQTT Configuration:
1. Broker: Use menuconfig **"MQTT Configuration"** to configure the broker url.


## API

### Steps to enable console in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. In the main file of the example, add the following line:
    ```c
    #include "console_mqtt.h"
    ```
3. Ensure esp-netif and NVS flash is initialized and default event loop is created in your app_main():
    ```c
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ```
4. In your app_main() function, add the following line as the last line:
    ```c
    ESP_ERROR_CHECK(console_cmd_init());     // Initialize console

    // Register all plugin command added to your project
    ESP_ERROR_CHECK(console_cmd_all_register());

    // To register only mqtt command skip calling console_cmd_all_register()
    ESP_ERROR_CHECK(console_cmd_mqtt_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

### Adding a plugin command or component:
To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

For more details refer [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)


## Suported command:

### mqtt:
```
mqtt  [-CD] [-h <host>] [-u <username>] [-P <password>]
  mqtt command
  -C, --connect  Connect to a broker
  -h, --host=<host>  Specify the host uri to connect to
  -u, --username=<username>  Provide a username to be used for authenticating with the broker
  -P, --password=<password>  Provide a password to be used for authenticating with the broker
  -D, --disconnect  Disconnect from the broker

mqtt_pub  [-t <topic>] [-m <message>]
  mqtt publish command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -m, --message=<message>  Message to Publish

mqtt_sub  [-U] [-t <topic>]
  mqtt subscribe command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -U, --unsubscribe  Unsubscribe from a topic
```
