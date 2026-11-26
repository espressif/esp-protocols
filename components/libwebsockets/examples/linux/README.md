# libwebsockets on the Linux target

Builds libwebsockets for the ESP-IDF `linux` target (the host), against ESP-IDF's
mbedtls, and creates a client context with the mbedtls TLS backend. No network is
needed. CI builds and runs it to keep the linux build of the component working.

```bash
idf.py --preview set-target linux
idf.py build
./build/lws_linux.elf
```

The linux target needs `CONFIG_LWIP_ENABLE=n`, which is ESP-IDF's default there.
lws uses the host's sockets on this target, not lwip. The component stops the
build with an error if lwip is enabled.
