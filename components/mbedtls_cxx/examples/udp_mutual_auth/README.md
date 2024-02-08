# UDP Mutual authentication example

This example uses `mbedtls_cxx` to perform a DTLS handshake and exchange a message between server and client.
The example uses UDP sockets on `'localhost'` interface, so no actual connection is needed, it could be run on linux target as well as on ESP32.
