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
3. Ensure esp-netif is initialized and default event loop is created in your app_main():
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

Note: Auto-registration of a specific plugin command can be disabled from menuconfig.

### Certificate Integration for Mutual Authentication
To enhance security and enable secure communication over MQTT, three functions have been added to the API, allowing users to set client certificates, client keys, and broker certificates separately.

Setting the client certificate:
```c
set_mqtt_client_cert(client_cert_pem_start, client_cert_pem_end);
```
Setting the client key:
```c
set_mqtt_client_key(client_key_pem_start, client_key_pem_end);
```
Setting the broker certificate:
```c
set_mqtt_broker_certs(broker_cert_pem_start, broker_cert_pem_end);
```
Each function takes pointers to the start and end of the respective PEM-encoded data, allowing users to specify the necessary certificate and key information independently. For a complete secure MQTT setup, users should call all three functions in their application code.

To utilize these certificates, users need to include additional arguments when establishing MQTT connections using the library. Specifically, users should provide the `--cert`, `--key`, and `--cafile` options along with the MQTT connection command.

### Adding a plugin command or component:
To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

For more details refer [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)

## Suported command:

### mqtt:
```
mqtt  [-CsD] [-h <host>] [-u <username>] [-P <password>] [--cert] [--key] [--cafile]
  mqtt command
  -C, --connect  Connect to a broker (flag, no argument)
  -h, --host=<host>  Specify the host uri to connect to
  -s, --status  Displays the status of the mqtt client (flag, no argument)
  -u, --username=<username>  Provide a username to be used for authenticating with the broker
  -P, --password=<password>  Provide a password to be used for authenticating with the broker
      --cert  Define the PEM encoded certificate for this client, if required by the broker (flag, no argument)
      --key  Define the PEM encoded private key for this client, if required by the broker (flag, no argument)
      --cafile  Define the PEM encoded CA certificates that are trusted (flag, no argument)
      --use-internal-bundle  Use the internal certificate bundle for TLS (flag, no argument)
  -D, --disconnect  Disconnect from the broker (flag, no argument)

mqtt_pub  [-t <topic>] [-m <message>]
  mqtt publish command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -m, --message=<message>  Message to Publish

mqtt_sub  [-U] [-t <topic>]
  mqtt subscribe command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -U, --unsubscribe  Unsubscribe from a topic
```
