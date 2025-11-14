Advanced esp-modem use cases
============================

This chapter outlines basic extensibility of the esp-modem component.

.. _dce_factory:

Custom instantiation with DCE factory
--------------------------------------

It is possible to create a modem handle in many different ways:

- Build a DCE on top a generic module, user defined module or build the module only (in case the application will only use AT command interface)
- Create the DCE as a shared, unique or a vanilla pointer
- Create a generic DCE or a templated DCE_T of a specific module (this could be one of the supported modules or a user defined module)

All the functionality is provided by the DCE factory

.. doxygengroup:: ESP_MODEM_DCE_FACTORY
   :members:

.. _create_custom_module:

Create custom module
--------------------

Creating a custom module is necessary if the application needs to use a specific device that is not supported
and their commands differ from any of the supported devices. In this case it is recommended to define a new class
representing this specific device and derive from the :cpp:class:`esp_modem::GenericModule` (or any other available
module, that's close to your custom device). Then you can create the DCE using :ref:`the DCE factory<dce_factory>`
public method :cpp:func:`esp_modem::dce_factory::Factory::create_unique_dce_from`.

Please note that the ``pppos_client`` example defines a trivial custom DCE which overrides one command, and adds a new command
for demonstration purposes only.

It is also possible to create a specific DCE class that would conform to the generic ``DCE`` API and use all generic commands,
work with commands differently. This might be useful to add some custom preprocessing of commands or replies.
Please check the ``modem_console`` example with ``CONFIG_EXAMPLE_MODEM_DEVICE_SHINY=y`` configuration which demonstrates
overriding default ``command()`` method to implement URC processing in user space.

Enhanced URC (Unsolicited Result Code) Handling
------------------------------------------------

The ESP modem library provides two interfaces for handling URCs: a legacy callback-based interface and an enhanced interface with granular buffer consumption control. The enhanced interface, available through :cpp:func:`esp_modem::DCE_T::set_enhanced_urc`, provides complete buffer visibility and allows URC handlers to make precise decisions about buffer consumption. This is particularly useful for processing multi-part responses or when you need to consume only specific portions of the buffer while preserving other data for command processing.

The enhanced URC handler receives a :cpp:struct:`esp_modem::DTE::UrcBufferInfo` structure containing the complete buffer context, including what data has been processed, what's new, and whether a command is currently active. The handler returns a :cpp:struct:`esp_modem::DTE::UrcConsumeInfo` structure specifying how much of the buffer to consume: none (wait for more data), partial (consume specific amount), or all (consume entire buffer). This granular control enables sophisticated URC processing scenarios such as line-by-line parsing of chunked HTTP responses or selective processing based on command state.

For applications migrating from the legacy URC interface, the enhanced interface maintains backward compatibility while providing significantly more control over buffer management. The legacy :cpp:func:`esp_modem::DCE_T::set_urc` method continues to work as before, but new applications should consider using the enhanced interface for better buffer control and processing flexibility.

Create new communication interface
----------------------------------

In order to connect to a device using an unsupported interface (e.g. SPI or I2C), it is necessary to implement
a custom DTE object and supply it into :ref:`the DCE factory<dce_factory>`. The DCE is typically created in two steps:

- Define and create the corresponding terminal, which communicates on the custom interface. This terminal should support basic IO methods defined in :cpp:class:`esp_modem::Terminal` and derive from it.
- Create the DTE which uses the custom Terminal

Please refer to the implementation of the existing UART DTE.
