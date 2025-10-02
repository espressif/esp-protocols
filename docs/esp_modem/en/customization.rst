Customization
=============

This chapter covers how to customize ESP-MODEM for your specific requirements by creating custom modules and adding new commands.

Custom Module Development
-------------------------

For most customization needs, you don't need development mode. Instead, you can create custom modules that inherit from existing ESP-MODEM classes.

Creating Custom Modules
~~~~~~~~~~~~~~~~~~~~~~~

The recommended approach for adding support for new modem modules or custom commands is to create a custom module class. This approach:

- **Doesn't require development mode**
- **Keeps your changes separate** from the core library
- **Allows easy updates** of the ESP-MODEM library
- **Provides full flexibility** for custom commands

Basic Custom Module Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here's a simple example of creating a custom module:

.. code-block:: cpp

    #include "cxx_include/esp_modem_api.hpp"
    #include "cxx_include/esp_modem_command_library_utils.hpp"

    class MyCustomModule: public GenericModule {
        using GenericModule::GenericModule;

    public:
        // Add a new command
        command_result get_custom_info(std::string &info) {
            return esp_modem::dce_commands::generic_get_string(
                dte.get(), "AT+CUSTOM?\r", info);
        }

        // Override an existing command
        command_result get_signal_quality(int &rssi, int &ber) override {
            // Custom implementation
            return esp_modem::dce_commands::generic_get_string(
                dte.get(), "AT+CSQ\r", rssi, ber);
        }
    };

Using Custom Modules with C++ API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With the C++ API, you can use your custom module directly:

.. code-block:: cpp

    // Create DCE with custom module
    auto dce = dce_factory::Factory::create_unique_dce_from<MyCustomModule>(
        dce_config, std::move(dte), netif);

    // Use custom commands
    std::string info;
    auto result = dce->get_custom_info(info);

Using Custom Modules with C API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To use custom modules with the C API, you need to:

1. **Enable custom module support** in Kconfig:

   .. code-block:: bash

       idf.py menuconfig
       # Navigate to: Component config → ESP-MODEM
       # Enable: "Add support for custom module in C-API"

2. **Create a custom module header** (e.g., ``custom_module.hpp``) in your main component

3. **Implement the required functions**:

   .. code-block:: cpp

       // Create custom DCE function
       DCE *esp_modem_create_custom_dce(
           const esp_modem_dce_config_t *dce_config,
           std::shared_ptr<DTE> dte,
           esp_netif_t *netif) {
           return dce_factory::Factory::create_unique_dce_from<MyCustomModule, DCE *>(
               dce_config, std::move(dte), netif);
       }

       // Add C API wrappers for custom commands
       extern "C" esp_err_t esp_modem_get_custom_info(esp_modem_dce_t *dce_wrap, char *info) {
           if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
               return ESP_ERR_INVALID_ARG;
           }
           std::string info_str{CONFIG_ESP_MODEM_C_API_STR_MAX};
           auto ret = command_response_to_esp_err(
               static_cast<MyCustomModule *>(dce_wrap->dce->get_module())->get_custom_info(info_str));
           if (ret == ESP_OK && !info_str.empty()) {
               strlcpy(info, info_str.c_str(), CONFIG_ESP_MODEM_C_API_STR_MAX);
           }
           return ret;
       }

4. **Use the custom commands** in your C code:

   .. code-block:: c

       char info[128];
       esp_err_t ret = esp_modem_get_custom_info(dce, info);

Complete Example
~~~~~~~~~~~~~~~~

See the ``examples/pppos_client`` example for a complete demonstration of custom module development. This example shows:

- Creating a custom module that inherits from ``GenericModule``
- Adding new commands (``get_time()``)
- Overriding existing commands (``get_signal_quality()``)
- Integration with both C++ and C APIs

Available Base Classes
----------------------

You can inherit from several base classes depending on your needs:

**GenericModule**
    The most general implementation of a common modem. Use this when:
    - Your modem supports most standard AT commands
    - You need to add a few custom commands
    - You want to override some existing commands

**Specific Module Classes**
    Inherit from existing module classes (e.g., ``SIM800``, ``BG96``, ``SIM7600``) when:
    - Your modem is very similar to an existing one
    - You only need minor modifications
    - You want to leverage existing device-specific optimizations

**ModuleIf**
    Use this minimal interface when:
    - You only need basic AT command functionality
    - You don't need network interface features
    - You want to implement a custom DTE without DCE

Command Utilities
-----------------

ESP-MODEM provides utility functions to help implement custom commands:

**Generic Command Helpers**
    - ``generic_get_string()`` - Parse string responses
    - ``generic_get_int()`` - Parse integer responses
    - ``generic_set_string()`` - Send string commands
    - ``generic_set_int()`` - Send integer commands

**Response Parsing**
    - ``get_number_from_string()`` - Extract numbers from responses
    - ``get_string_from_response()`` - Extract strings from responses
    - ``get_urc()`` - Handle unsolicited result codes

Example Usage:
.. code-block:: cpp

    // Get a string value
    command_result get_imei(std::string &imei) {
        return esp_modem::dce_commands::generic_get_string(
            dte.get(), "AT+CGSN\r", imei);
    }

    // Get an integer value
    command_result get_signal_strength(int &rssi) {
        return esp_modem::dce_commands::generic_get_int(
            dte.get(), "AT+CSQ\r", rssi);
    }

Best Practices
--------------

**For Application Developers:**
- Use production mode for better IDE support and faster builds
- Create custom modules for new modem support
- Inherit from ``GenericModule`` or other existing modules
- Keep customizations in your project, not in the ESP-MODEM library

**Module Design:**
- Choose the most appropriate base class for your needs
- Override only the commands you need to modify
- Use the provided utility functions for common operations
- Follow the existing command naming conventions

**Testing:**
- Test your custom module with real hardware
- Verify compatibility with existing ESP-MODEM features
- Test both C++ and C API usage if applicable
- Consider edge cases and error handling

**Documentation:**
- Document your custom commands clearly
- Provide usage examples
- Explain any device-specific requirements
- Note any limitations or known issues
