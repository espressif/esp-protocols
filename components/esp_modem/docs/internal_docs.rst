DCE Internal implementation
===========================

This chapter provides a detailed description of the classes and building blocks of the esp-modem component and their responsibilities.

The esp-modem actually implements the DCE class, which in turn aggregates these thee units:

- :ref:`DTE<dte_impl>` to communicate with the device on a specific Terminal interface such as UART.
- :ref:`Netif<netif_impl>` to provide the network connectivity
- :ref:`Module<module_impl>` to define the specific command library

Developers would typically have to

* Add support for a new module
* Implement a generic (common for all modules) AT command

This is explained in the :ref:`Module<module_impl>` section, as :ref:`Adding new module or command<module_addition>`

------------

.. doxygengroup:: ESP_MODEM_DCE
   :members:

.. _dte_impl:

DTE abstraction
---------------

DTE is a basic unit to talk to the module using a Terminal interface. It also implements and uses the CMUX to multiplex
terminals. Besides the DTE documentation, this section also refers to the

- :ref:`Terminal interface<term_impl>`
- :ref:`CMUX implementation<cmux_impl>`


------------

.. doxygengroup:: ESP_MODEM_DTE
   :members:

.. _term_impl:

Terminal interface
^^^^^^^^^^^^^^^^^^

.. doxygengroup:: ESP_MODEM_TERMINAL
   :members:

.. _cmux_impl:

CMUX implementation
^^^^^^^^^^^^^^^^^^^

.. doxygengroup:: ESP_MODEM_CMUX
   :members:

.. _netif_impl:

Netif
-----

.. doxygengroup:: ESP_MODEM_NETIF
   :members:

.. _module_impl:

Module abstraction
------------------

.. doxygengroup:: ESP_MODEM_MODULE
   :members:

.. _module_addition:

Adding new devices
^^^^^^^^^^^^^^^^^^

To support a new module, developers would have to implement a new class derived from :cpp:class:`esp_modem::GenericModule` the same way
as it is described in the  :ref:`Advanced user manual<create_custom_module>`. The only difference is that the new class (and factory extension)
would be available in the esp_modem code base.

Implement a new generic command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Adding a generic command, i.e. the command that is shared for all modules and is included in the :cpp:class:`esp_modem::GenericModule`,
has to be declared first in the ``include/generate/esp_modem_command_declare.inc`` file, which is the single source
of supported command definitions, that is used in:

* public C API
* public CPP API
* generated documentation
* implementation of the command

Therefore, a care must be taken, to correctly specify all parameters and types, especially:

* Keep number of parameters low (<= 6, used in preprocessor's forwarding to the command library)
* Use macros to specify parameter types (as they are used both from C and C++ with different underlying types)
* Parameter names are used only for clarity and documentation, they get expanded to numbered arguments.

Please use the following pattern: ``INT_IN(p1, baud)``, meaning that the parameter is an input integer,
human readable argument name is ``baud``, it's the first argument, so expands to ``p1`` (second argument would be ``p2``, etc)

Command library
^^^^^^^^^^^^^^^

This is a namespace holding a library of typical AT commands used by supported devices.
Please refer to the :ref:`c_api` for the list of supported commands.

.. doxygengroup:: ESP_MODEM_DCE_COMMAND
   :members:


Modem types
-----------

.. doxygengroup:: ESP_MODEM_TYPES
   :members:
