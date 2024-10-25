ASIO port
=========

Overview
--------
Asio is a cross-platform C++ library, see https://think-async.com/Asio/. It provides a consistent asynchronous model using a modern C++ approach.


ASIO documentation
^^^^^^^^^^^^^^^^^^
Please refer to the original asio documentation at https://think-async.com/Asio/Documentation.
Asio also comes with a number of examples which could be find under Documentation/Examples on that web site.

Supported features
^^^^^^^^^^^^^^^^^^
ESP platform port currently supports only network asynchronous socket operations; does not support serial port.
SSL/TLS support is disabled by default and could be enabled in component configuration menu by choosing TLS library from

- mbedTLS with OpenSSL translation layer (default option)

SSL support is very basic at this stage and it does include following features:

- Verification callbacks
- DH property files
- Certificates/private keys file APIs

Internal asio settings for ESP include

- EXCEPTIONS are enabled in ASIO if enabled in menuconfig
- TYPEID is enabled in ASIO if enabled in menuconfig

Application Example
-------------------
ESP examples are based on standard asio :component:`asio/examples`:

- :component:`asio/examples/asio_chat`
- :component:`asio/examples/async_request`
- :component:`asio/examples/socks4`
- :component:`asio/examples/ssl_client_server`
- :component:`asio/examples/tcp_echo_server`
- :component:`asio/examples/udp_echo_server`

Please refer to the specific example README.md for details
