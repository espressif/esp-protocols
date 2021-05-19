DCE Internal implementation
===========================

This chapter provides a detailed description of the classes and building blocks of the esp-modem component and their responsibilities.

The esp-modem actually implements the DCE class, which in turn aggregates these thee units:

- :ref:`DTE<dte_impl>` to communicate with the device on a specific Terminal interface such as UART.
- :ref:`Netif<netif_impl>` to provide the network connectivity
- :ref:`Module<module_impl>` to define the specific command library

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





