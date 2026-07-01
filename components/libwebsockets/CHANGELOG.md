# Changelog

## [4.3.3~3](https://github.com/espressif/esp-protocols/commits/lws-v4.3.3_3)

### Features

- Bump libwebsockets submodule to warmcat main, which includes mbedTLS 4 support and the FreeRTOS/ESP-IDF v6 build fixes
- Allow building with IDF v6.0 (ships mbedTLS 4 by default)
- Force lws's mbedTLS 4 code path on IDF: lws's own V4 probe is skipped on FreeRTOS, so detect the version from the IDF mbedtls component and inject the tf-psa-crypto include dirs when present

## [4.3.3~2](https://github.com/espressif/esp-protocols/commits/lws-v4.3.3_2)

### Features

- add Kconfig toggles + bump submodule ([23448d54](https://github.com/espressif/esp-protocols/commit/23448d54))

## [4.3.3~1](https://github.com/espressif/esp-protocols/commits/lws-v4.3.3_1)

### Bug Fixes

- Remove lws support for IDF>=v6.0 ([b70cc3fc](https://github.com/espressif/esp-protocols/commit/b70cc3fc))
- Update websocket Echo server (#894) ([318e41b3](https://github.com/espressif/esp-protocols/commit/318e41b3))
- Adds missing license info ([7ea6879a](https://github.com/espressif/esp-protocols/commit/7ea6879a))

### Updated

- chore(lws): fixed formatting ([91e7e9fa](https://github.com/espressif/esp-protocols/commit/91e7e9fa))

## [4.3.3](https://github.com/espressif/esp-protocols/commits/lws-v4.3.3)

### Features

- Add initial support libwebsockets component ([ef3443d](https://github.com/espressif/esp-protocols/commit/ef3443d))
