# Simple Console Initializer
The component provides a simple api's to initialize and start the esp console.
It also provides an api to register an user provided command.

## API

### Steps to enable console in an example code:
1. Add this component to your project using ```idf.py add-dependency``` command.
2. In the main file of the example, add the following line:
    ```c
    #include "console_simple_init.h"
    ```
3. Ensure NVS flash is initialized and default event loop is created in your app_main():
    ```c
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

    // Define the function prototype for do_user_cmd
    // It's a function that takes an integer (argc) and a pointer to a pointer to char (argv)
    int do_user_cmd(int argc, char **argv);

    // Register the do_user_cmd function as a command callback function for "user" command
    // This allows you to execute the do_user_cmd function when the "user" command is invoked
    ESP_ERROR_CHECK(console_cmd_user_register("user", do_user_cmd));


    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```
