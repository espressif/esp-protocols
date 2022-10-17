.. _c_api:

API Guide for C interface
=========================


C API is very simple and consist of these two basic parts:

- :ref:`lifecycle_api` 
- :ref:`modem_commands`

Typical application workflow is to:

- Create a DCE instance (using :cpp:func:`esp_modem_new`)
- Call specific functions to issue AT commands (:ref:`modem_commands`)
- Switch to the data mode (using :cpp:func:`esp_modem_set_mode`)
- Perform desired network operations (using standard networking API, unrelated to ESP-MODEM)
- Optionally switch back to command mode (again :cpp:func:`esp_modem_set_mode`)
- Destroy the DCE handle (sing :cpp:func:`esp_modem_destroy`)

Note the configuration structures for DTE and DCE, needed for creating the DCE instance, is documented in :ref:`api_config`

.. _lifecycle_api:

Lifecycle API
-------------

These functions are used to create, destroy and set modem working mode.

- :cpp:func:`esp_modem_new`
- :cpp:func:`esp_modem_destroy`
- :cpp:func:`esp_modem_set_mode`

.. doxygengroup:: ESP_MODEM_C_API


.. _modem_commands:

Modem commands
--------------

These functions are the actual commands to communicate with the modem using AT command interface.


.. doxygenfile:: esp_modem_api_commands.h

.. _api_config:

Configuration structures
------------------------


.. doxygengroup:: ESP_MODEM_CONFIG
   :members: