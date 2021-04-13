C++ API Documentation
=====================

Similar to the :ref:`c_api`, the basic application workflow consist of

- Construction of the DCE
- Sending (AT) commands
- Switching modes
- Destroying the DCE

Create DTE and DCE
------------------

.. doxygengroup:: ESP_MODEM_INIT_DTE

.. doxygengroup:: ESP_MODEM_INIT_DCE


Mode switching commands
-----------------------

.. doxygenclass:: esp_modem::DCE_T
   :members:


Modem commands
--------------

Create the DCE object with DCE factory :cpp:func:`esp_modem_new`

.. doxygenclass:: DCE
   :members:


Destroy the DCE
---------------

The DCE object is created as ``std::unique_ptr`` by default and as such doesn't have to be explicitly destroyed.
It simply gets destroyed and cleaned-up automatically if the object goes out of the block scope.