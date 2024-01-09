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

Create new communication interface
----------------------------------

In order to connect to a device using an unsupported interface (e.g. SPI or I2C), it is necessary to implement
a custom DTE object and supply it into :ref:`the DCE factory<dce_factory>`. The DCE is typically created in two steps:

- Define and create the corresponding terminal, which communicates on the custom interface. This terminal should support basic IO methods defined in :cpp:class:`esp_modem::Terminal` and derive from it.
- Create the DTE which uses the custom Terminal

Please refer to the implementation of the existing UART DTE.
