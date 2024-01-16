# Console command iperf
The component provides a console where the 'iperf' command can be executed for any example project.

## API

### Steps to enable console in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. In the main file of the example, add the following line:
    ```c
    #include "console_iperf.h"
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

    // To register only iperf command skip calling console_cmd_all_register()
    ESP_ERROR_CHECK(console_cmd_iperf_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

### Adding a plugin command or component:
To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

For more details refer [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)


## Suported command:

### iperf:
```
iperf  [-suVa] [-c <ip>] [-p <port>] [-l <length>] [-i <interval>] [-t <time>] [-b <bandwidth>]
  Command to measure network performance, through TCP or UDP connections.
  -c, --client=<ip>  run in client mode, connecting to <host>
  -s, --server  run in server mode
     -u, --udp  use UDP rather than TCP
  -V, --ipv6_domain  use IPV6 address rather than IPV4
  -p, --port=<port>  server port to listen on/connect to
  -l, --len=<length>  set read/write buffer size
  -i, --interval=<interval>  seconds between periodic bandwidth reports
  -t, --time=<time>  time in seconds to transmit for (default 10 secs)
  -b, --bandwidth=<bandwidth>  bandwidth to send at in Mbits/sec
   -a, --abort  abort running iperf
```
