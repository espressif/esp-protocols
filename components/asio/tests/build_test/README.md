# asio build-only regression test

This test reproduces the build issue reported in [PR 2403](https://github.com/espressif/esp-protocols/issues/2403):

> `asio.hpp` includes `sock_utils`' `socketpair.h`, but `components/asio/CMakeLists.txt`
> used `PRIV_REQUIRES sock_utils`, so downstream components that only declared
> `REQUIRES asio` failed to compile with `socketpair.h: No such file or directory`.

The test project defines a custom component `asio_consumer` (under
`components/asio_consumer/`) that:

- Only declares `REQUIRES asio` in its `CMakeLists.txt` (not `sock_utils`).
- Includes `asio.hpp` from a `.cpp` source.

Before the fix (where `sock_utils` was in `PRIV_REQUIRES`) the build fails at the
preprocessor stage. After the fix (`sock_utils` in `REQUIRES` and `public: true`
in `idf_component.yml`) the build succeeds.

Build with:

```bash
idf.py -DIDF_TARGET=esp32 build
```
