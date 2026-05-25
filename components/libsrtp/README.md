# libsrtp

[![Component Registry](https://components.espressif.com/components/espressif/libsrtp/badge.svg)](https://components.espressif.com/components/espressif/libsrtp)

[libsrtp](https://github.com/cisco/libsrtp) (Cisco) port for ESP-IDF using the mbedtls crypto backend with AES-GCM.

Used for SRTP/SRTCP packet protection in WebRTC and other RTP-based protocols on ESP32-series chips.

> **Not the same as `espressif/esp_libsrtp`.** The existing `espressif/esp_libsrtp` component is a pre-built closed-source libSRTP binary distributed via `esp-adf-libs` (Custom license). `libsrtp` is an open-source wrapper around upstream [`cisco/libsrtp` 2.x](https://github.com/cisco/libsrtp) — built from source as part of your project, BSD-3-Clause upstream + Apache-2.0 port code. Pick `libsrtp` when you need the source-built, open-source path (e.g. for WebRTC integrations); pick `esp_libsrtp` when you're already on the ADF binary stack.

## Features

- libsrtp [`v2.8.0`](https://github.com/cisco/libsrtp/releases/tag/v2.8.0) (commit `24b3bf8`)
- mbedtls crypto backend (AES-CM, AES-GCM, HMAC-SHA1) — IDF's mbedTLS routes AES through the on-chip AES peripheral when `CONFIG_MBEDTLS_HARDWARE_AES=y` (default)
- AEAD profiles: `AEAD_AES_128_GCM`, `AEAD_AES_256_GCM`
- SRTP profiles: `AES_CM_128_HMAC_SHA1_80`, `AES_CM_128_HMAC_SHA1_32`

## Add to a project

```bash
idf.py add-dependency "espressif/libsrtp^2.8.0"
```

Or in your project's `main/idf_component.yml`:

```yaml
dependencies:
  espressif/libsrtp: "^2.8.0"
```

Requires ESP-IDF ≥ 5.4.

## Tests

- `test_apps/` — embedded Unity smoke test (init/shutdown + version). Run with
  `pytest_libsrtp.py` against `esp32` / `esp32c3`.
- `host_test/` — IDF Linux-target binary that performs an AES-GCM-128
  protect/unprotect roundtrip. Run with `pytest_libsrtp_linux.py`.

## Source

This wrapper bundles [cisco/libsrtp](https://github.com/cisco/libsrtp) as a git submodule pinned at the upstream release tag. All SRTP cryptographic implementation is upstream; this repo adds only the ESP-IDF build glue (`CMakeLists.txt`, `port/config.h`) plus one small port-side delta in `port/crypto_kernel.c` (replaces the upstream `crypto/kernel/crypto_kernel.c`) to opt out of the AES-ICM-192 cipher registration when GCM is enabled. Re-port from upstream when bumping the libsrtp submodule.

## License

`Apache-2.0 AND BSD-3-Clause` — the ESP-IDF port glue under this repo (`CMakeLists.txt`, `port/`, `Kconfig`, build scripts) is Apache-2.0 (see `LICENSE`); the bundled `libsrtp/` submodule remains under upstream's BSD-3-Clause (see `libsrtp/LICENSE`).
