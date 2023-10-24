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

    // Register any other plugin command added to your project
    ESP_ERROR_CHECK(console_cmd_all_register());

    ESP_ERROR_CHECK(console_cmd_start());    // Start console
    ```

### Automatic registration of console commands
The `console_simple_init` component includes a utility function named `console_cmd_all_register()`. This function automates the registration of all commands that are linked into the application. To use this functionality, the application can call `console_cmd_all_register()` as demonstrated above.

When creating a new component, you can ensure that its commands are registered automatically by placing the registration function into the `.console_cmd_desc` section within the output binary.

To achieve this, follow these steps:
1. Add the following lines to the main file of the component
```
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "cmd_name_string",
    .plugin_regd_fn = &cmd_registration_function
};̌
```
2. Add the `WHOLE_ARCHIVE` flag to CMakeLists.txt of the component.


For more details refer:
* [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)
* [Linker Script Generation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/linker-script-generation.html)
