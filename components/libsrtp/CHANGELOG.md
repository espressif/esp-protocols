# Changelog

## [2.8.0](https://github.com/espressif/esp-protocols/commits/libsrtp-v2.8.0)

### Features

- First release. Tracks upstream cisco/libsrtp [`v2.8.0`](https://github.com/cisco/libsrtp/releases/tag/v2.8.0) (commit `24b3bf8`); component version follows upstream.
- Wraps libsrtp as an ESP-IDF component using mbedTLS for AES-ICM, AES-GCM and HMAC-SHA1. ESP-IDF's mbedTLS routes AES through the on-chip AES peripheral by default (`CONFIG_MBEDTLS_HARDWARE_AES=y`), so SRTP protect/unprotect leverages hardware crypto with no wrapper-side code.
- One small port-side delta in `port/crypto_kernel.c`: opts out of the AES-ICM-192 cipher registration when GCM is enabled (saves binary size; AES-CM-128 + AES-GCM cover all WebRTC SRTP suites).
- Embedded smoke test (`test_apps/`) and host-side AES-GCM-128 protect/unprotect roundtrip test (`host_test/`) included.
