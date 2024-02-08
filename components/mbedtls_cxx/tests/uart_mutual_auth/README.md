# UART mutual authentication test

This test creates a (D)TLS server and a client on one device and checks if they can perform a TLS handshake and exchange a message.
The test uses UART as the physical layer of communication channel and, since it runs on a single ESP32, it expects two UART ports interconnected as per below:

```
    +------------------------+
    |   ESP32                |
    |                        |
    |   UART-1      UART-2   |
    +---25---26------4---5---+
        |    |       |   |
        |    +-------+   |
        +----------------+
```

The test runs in two configurations: TLS and DTLS.
