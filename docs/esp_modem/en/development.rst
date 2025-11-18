Development
===========

This chapter covers development mode, build system, and workflow for ESP-MODEM library developers.

Development Mode
----------------

ESP-MODEM supports two different modes for handling AT command definitions, each optimized for different use cases.

Production Mode (Default)
~~~~~~~~~~~~~~~~~~~~~~~~~~

Production mode uses pre-generated headers and sources from the ``command/`` directory. This mode is ideal for:

- **Application development** using existing AT commands
- **Better IDE navigation** and code completion
- **Faster compilation times** with stable, tested implementations
- **End users** who don't need to modify the core library

Development Mode
~~~~~~~~~~~~~~~~

Development mode uses in-place macro expansion from the ``generate/`` directory. This mode is designed for:

- **ESP-MODEM library developers** contributing to the core library
- **Modifying core AT command definitions** in ``esp_modem_command_declare.inc``
- **Debugging command implementations** at the source level
- **Working directly with command source definitions**

To enable development mode:

.. code-block:: bash

    idf.py menuconfig
    # Navigate to: Component config → ESP-MODEM
    # Enable: "Use development mode"

Or set the configuration directly:

.. code-block:: bash

    idf.py -D CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE=y build

.. note::
    Development mode requires C preprocessor expansion at compile time, which may result in longer compilation times and larger binary sizes.

When to Use Development Mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Use Development Mode when:**
- Contributing to the ESP-MODEM library itself
- Modifying core AT command definitions in ``esp_modem_command_declare.inc``
- Debugging issues in the command library
- Adding new commands to the core library for all users

**Use Production Mode when:**
- Developing applications with ESP-MODEM
- Adding support for new modem modules (see :ref:`customization`)
- Creating custom commands for specific projects
- General application development

Build System
------------

The ESP-MODEM build system automatically handles the differences between production and development modes:

- **Production mode**: Uses pre-generated sources from ``command/`` directory
- **Development mode**: Uses macro expansion from ``generate/`` directory

The build system selects the appropriate source directory based on the ``CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE`` configuration.

Directory Structure
~~~~~~~~~~~~~~~~~~~

ESP-MODEM uses two parallel directory structures:

**``generate/`` Directory (Development Mode)**
    Contains source files with macro definitions that get expanded at compile time:

    - ``generate/include/esp_modem_command_declare.inc`` - Core AT command definitions
    - ``generate/include/cxx_include/`` - C++ header templates
    - ``generate/src/`` - Source file templates

**``command/`` Directory (Production Mode)**
    Contains pre-generated, expanded source files:

    - ``command/include/esp_modem_api.h`` - Generated C API
    - ``command/include/cxx_include/`` - Generated C++ headers
    - ``command/src/`` - Generated source files

Code Generation
~~~~~~~~~~~~~~~

ESP-MODEM uses a sophisticated code generation system to create the pre-generated sources. The ``generate.sh`` script:

- Processes AT command definitions from ``esp_modem_command_declare.inc``
- Uses C preprocessor metaprogramming to expand command prototypes
- Generates both C and C++ API headers
- Formats the generated code for consistency

Generate Script Usage
~~~~~~~~~~~~~~~~~~~~~

The ``generate.sh`` script is located in ``components/esp_modem/scripts/`` and can be used to:

**Generate all default files:**
.. code-block:: bash

    ./scripts/generate.sh

**Generate specific files:**
.. code-block:: bash

    ./scripts/generate.sh generate/include/cxx_include/esp_modem_command_library.hpp

**Generate files for documentation:**
.. code-block:: bash

    ./scripts/generate.sh ../../docs/esp_modem/generate/dce.rst

The script automatically:
- Determines the correct compiler (clang/clang++) based on file extension
- Expands macros using C preprocessor
- Formats generated code with astyle
- Handles different file types (.hpp, .cpp, .h, .rst)

Developer Workflow
------------------

Adding New AT Commands
~~~~~~~~~~~~~~~~~~~~~~

To add new AT commands to the core ESP-MODEM library:

1. **Enable development mode** in your project
2. **Edit** ``components/esp_modem/generate/include/esp_modem_command_declare.inc``
3. **Add your command definition** using the ``ESP_MODEM_DECLARE_DCE_COMMAND`` macro:

   .. code-block:: c

       /**
        * @brief Your new command description
        * @param[in] param1 Description of parameter 1
        * @param[out] param2 Description of parameter 2
        * @return OK, FAIL or TIMEOUT
        */
       ESP_MODEM_DECLARE_DCE_COMMAND(your_new_command, command_result,
                                     STR_IN(param1), INT_OUT(param2))

4. **Test your changes** in development mode
5. **Generate production files** using ``generate.sh``
6. **Test in production mode** to ensure compatibility
7. **Submit your changes** with appropriate tests

Command Definition Macros
~~~~~~~~~~~~~~~~~~~~~~~~~~

ESP-MODEM provides several macros for defining commands:

- ``ESP_MODEM_DECLARE_DCE_COMMAND`` - Standard command declaration
- ``STR_IN(param)`` - String input parameter
- ``STR_OUT(param)`` - String output parameter
- ``INT_IN(param)`` - Integer input parameter
- ``INT_OUT(param)`` - Integer output parameter

Testing and Validation
~~~~~~~~~~~~~~~~~~~~~~

When developing ESP-MODEM library changes:

1. **Test in both modes** - Ensure your changes work in both development and production modes
2. **Run existing tests** - Verify you don't break existing functionality
3. **Test with multiple modules** - Ensure compatibility across different modem modules
4. **Validate generated code** - Check that generated files are correct and properly formatted
5. **Update documentation** - Add documentation for new commands or features

CI/CD Integration
~~~~~~~~~~~~~~~~~

The ESP-MODEM project includes automated testing that:

- **Validates generated files** - Ensures generated sources are up-to-date
- **Tests both modes** - Runs tests in both development and production modes
- **Checks formatting** - Validates code formatting and style
- **Builds examples** - Ensures examples work with changes

Best Practices
--------------

**For Library Developers:**
- Use development mode when modifying core library files
- Test changes in both production and development modes
- Follow the existing code generation patterns
- Update documentation when adding new commands
- Include appropriate tests for new functionality

**For Contributors:**
- Submit changes that work in both modes
- Include appropriate tests
- Update relevant documentation
- Consider backward compatibility
- Follow the existing coding style and patterns
