Migration Guide
===============

ESP-MODEM v2.0 introduces production mode (default) with pre-generated sources for better IDE navigation. Previous behavior (development mode) requires explicit configuration.

Breaking Changes
----------------

**Production Mode (Default)**
- Uses pre-generated sources from ``command/`` directory
- Better IDE navigation and code completion
- Faster compilation

**Development Mode (Optional)**
- Uses macro expansion from ``generate/`` directory
- Enable with ``CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE=y``
- Required for modifying core ESP-MODEM files

Migration Steps
---------------

**Application Developers:**
No changes required. Production mode is default.

**Library Developers:**
Enable development mode:

.. code-block:: bash

    idf.py -D CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE=y build

**Custom ``*.inc`` Files:**
Use generation script:

.. code-block:: bash

    ./components/esp_modem/scripts/generate.sh your_file.inc

**Build:**
.. code-block:: bash

    idf.py fullclean
    idf.py build

New Features (Coming Soon)
--------------------------

**Better URC Handling**
- Enhanced unsolicited result code processing
- Existing URC code remains compatible

**AT-based Networking**
- AT command networking examples now supports multiple connections

Troubleshooting
---------------

**Build errors:** ``idf.py fullclean && idf.py build``

**No IDE completion:** Use production mode (default)

**Custom .inc files:** Use ``./components/esp_modem/scripts/generate.sh your_file.inc``

**Modify core files:** Enable ``CONFIG_ESP_MODEM_ENABLE_DEVELOPMENT_MODE=y``
