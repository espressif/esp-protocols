# mbedtls_cxx

This is a simplified C++ wrapper of mbedTLS for performing TLS and DTLS handshake and communication. This component allows for overriding low level IO functions (`send()` and `recv()`) and thus supporting TLS over various physical channels.

## mbedTLS Version Support

This wrapper supports both mbedTLS v3 (legacy API) and mbedTLS v4 (PSA Crypto API). The appropriate API is selected automatically at compile time based on the mbedTLS version.

### PSA Crypto Lifecycle (mbedTLS v4)

When using mbedTLS v4, this wrapper calls `psa_crypto_init()` during `Tls::init()`. This function is idempotent - it's safe to call multiple times, whether by this wrapper or by other code using mbedTLS directly.

**Important:** This wrapper does **not** call `mbedtls_psa_crypto_free()` on destruction. This is intentional to avoid breaking other code that may also be using PSA Crypto. If your application requires explicit cleanup of PSA resources (e.g., for memory leak detection), you should call `mbedtls_psa_crypto_free()` at application shutdown after all TLS operations and other mbedTLS usage is complete.

```cpp
// At application shutdown (if needed):
#include "psa/crypto_extra.h"
mbedtls_psa_crypto_free();
```
