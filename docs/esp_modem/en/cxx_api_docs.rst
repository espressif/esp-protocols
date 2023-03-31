C++ API Documentation
=====================

Similar to the :ref:`c_api`, the basic application workflow consist of

.. toctree::

- :ref:`Construction of the DCE<cpp_init>`
- :ref:`Switching modes<cpp_mode_switch>`
- :ref:`Sending (AT) commands<cpp_modem_commands>`
- :ref:`Destroying the DCE<cpp_destroy>`

.. _cpp_init:

Create DTE and DCE
------------------

.. doxygengroup:: ESP_MODEM_INIT_DTE

.. doxygengroup:: ESP_MODEM_INIT_DCE


.. _cpp_mode_switch:

Mode switching commands
-----------------------

.. doxygenclass:: esp_modem::DCE_T
   :members:

.. _cpp_modem_commands:

Modem commands
--------------

.. include:: cxx_api_links.rst

.. _cpp_destroy:

Destroy the DCE
---------------

The DCE object is created as ``std::unique_ptr`` by default and as such doesn't have to be explicitly destroyed.
It simply gets destroyed and cleaned-up automatically if the object goes out of the block scope.
