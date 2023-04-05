ESP MQTT C++ client
====================

Overview
--------
The ESP MQTT client is a wrapper over the `esp_mqtt` client with the goal of providing a higher level API.

Features
--------
   * Supports MQTT version 3.11
   * Adds a Filter validation class for topic filters
   * Split the event handlers to member functions

Configuration
-------------

The current design uses exception as an error handling mechanism, therefore exceptions need to be enabled in menuconfig.

Usage
-----

User code needs to inherit fromm :cpp:class:`idf::mqtt::Client` and provide overloads for the event handlers.

.. note:: The handler is available to allow user code to interact directly with it in case of need. This member will likely be made private in the future once the class API stabilizes.


.. doxygenclass:: idf::mqtt::Client
    :members:
    :protected-members:

Event Handling
--------------

Events are dispatched throug calls to member functions each one dedicated to a type of event.

Application Example
-------------------

* :example:`tcp <../../components/esp_mqtt_cxx/examples/tcp>`
* :example:`ssl <../../components/esp_mqtt_cxx/examples/ssl>`

API Reference
-------------

Header File
^^^^^^^^^^^

* :project_file:`include/esp_mqtt.hpp`

Structures
^^^^^^^^^^

.. doxygenstruct:: idf::mqtt::MQTTException
    :members:

.. doxygenstruct:: idf::mqtt::Message
    :members:

Classes
^^^^^^^


.. doxygenclass:: idf::mqtt::Filter
    :members:

Header File
^^^^^^^^^^^

* :project_file:`include/esp_mqtt_client_config.hpp`

Structures
^^^^^^^^^^

.. doxygenstruct:: idf::mqtt::Host
    :members:

.. doxygenstruct:: idf::mqtt::URI
    :members:

.. doxygenstruct:: idf::mqtt::BrokerAddress
    :members:

.. doxygenstruct:: idf::mqtt::PEM
    :members:

.. doxygenstruct:: idf::mqtt::DER
    :members:

.. doxygenstruct:: idf::mqtt::Insecure
    :members:

.. doxygenstruct:: idf::mqtt::GlobalCAStore
    :members:

.. doxygenstruct:: idf::mqtt::PSK
    :members:

.. doxygenstruct:: idf::mqtt::Password
    :members:

.. doxygenstruct:: idf::mqtt::ClientCertificate
    :members:

.. doxygenstruct:: idf::mqtt::SecureElement
    :members:

.. doxygenstruct:: idf::mqtt::DigitalSignatureData
    :members:

.. doxygenstruct:: idf::mqtt::BrokerConfiguration
    :members:

.. doxygenstruct:: idf::mqtt::ClientCredentials
    :members:

.. doxygenstruct:: idf::mqtt::Event
    :members:

.. doxygenstruct:: idf::mqtt::LastWill
    :members:

.. doxygenstruct:: idf::mqtt::Session
    :members:

.. doxygenstruct:: idf::mqtt::Task
    :members:

.. doxygenstruct:: idf::mqtt::Connection
    :members:

.. doxygenstruct:: idf::mqtt::Configuration
    :members:
