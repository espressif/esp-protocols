# Simple Console Initializer
The component provides a simple api's to initialize and start the esp console.
It also provides an api to register an user provided command.

Existing applications need no changes. `console_cmd_init()`, `console_cmd_user_register()`, `console_cmd_all_register()`, and `console_cmd_start()` behave as before.

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

`console_cmd_user_register()` stores the help text `User defined command`. See `examples/console_config` for a custom prompt, help text, argtable, command context, and a plugin.

### Configuration

`console_cmd_init()` is `console_cmd_init_with_config(NULL)`. `CONSOLE_CMD_CONFIG_DEFAULT()` leaves every field zero, which keeps the ESP-IDF defaults: prompt `esp> `, 32 history lines, a 4096-byte task at priority 2, and no history file. A non-NULL prompt or history path, or a non-zero history length, stack size, priority, or command-line length, overrides that one field.

`task_core_id` of 0 means "do not override" and cannot pin the task to CPU 0. To request no affinity explicitly, pass `tskNO_AFFINITY`; do not pass `-1`, because that value is only correct on the SMP FreeRTOS kernel and asserts on the classic kernel. The field is ignored on ESP-IDF < 5.3. `max_cmdline_args` is ignored on ESP-IDF < 6.1.

`console_cmd_get_repl()` is NULL until init succeeds. Command, help, and hint pointers must stay valid until the console is deinitialized. A non-NULL hint is copied by ESP-IDF. An argtable passed to `console_cmd_register_with_args()` is only needed for the duration of the register call, unless the handler parses arguments with it later. `examples/console_config` keeps that argtable static for this reason.

```c
console_cmd_config_t config = CONSOLE_CMD_CONFIG_DEFAULT();
config.prompt = "cfg> ";
ESP_ERROR_CHECK(console_cmd_init_with_config(&config));
```

### Commands

`console_cmd_register()` takes a command name, help text, hint, and callback. `console_cmd_register_with_args()` leaves the hint NULL so ESP-IDF generates it from the argtable. `console_cmd_register_with_context()` is available on ESP-IDF >= 5.3 and passes a caller context pointer into the callback. `console_cmd_unregister()` returns `ESP_ERR_NOT_SUPPORTED` on ESP-IDF < 5.4. `console_cmd_run()` runs one command line without starting the interactive REPL.

### Automatic registration of console commands

`console_cmd_all_register()` registers every plugin linked into the application. `console_cmd_plugin_count()`, `console_cmd_plugin_foreach()`, and `console_cmd_register_plugin()` count, walk, or register one of those plugins.

In a plugin `.c` file, define the register function and then place its descriptor with the macro. `reg_fn` is a bare function name defined earlier in that file. The plugin name must be a string literal.

```c
static esp_err_t cmd_registration_function(void)
{
    return ESP_OK;
}

CONSOLE_CMD_REGISTER_PLUGIN("cmd_name_string", cmd_registration_function);
```

The linker drops a `.c` file that nothing calls. After `idf_component_register()` in the plugin component, force-link the global register function. The symbol is that function, not the static descriptor. `-u` pulls the object file in, and `linker.lf` keeps the `.console_cmd_desc` section.

```cmake
idf_component_get_property(console_dir console_simple_init COMPONENT_DIR)
include(${console_dir}/cmake/console_cmd_plugin.cmake)
console_cmd_plugin_force_link(${COMPONENT_LIB} console_cmd_wifi_register)
```

`WHOLE_ARCHIVE` still links a plugin. Prefer the helper: it keeps that one object file rather than the whole archive. An application component such as `main` is always linked, so it does not need the helper.

### Stop

`console_cmd_stop()` is compiled only when `CONSOLE_SIMPLE_INIT_ENABLE_STOP` is enabled. The default is n, because `esp_console_stop_repl()` pulls ESP-IDF's non-blocking linenoise and eventfd path into the link and changes console read behavior. It works on every supported ESP-IDF version: 5.5 and later call `esp_console_stop_repl()`, and older versions call the REPL `del()` handle, which is what that function wraps.

For more details refer:
* [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html)
* [Linker Script Generation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/linker-script-generation.html)
