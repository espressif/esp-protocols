# Changelog

## [5.0.0](https://github.com/espressif/esp-protocols/commits/lws-v5.0.0)

### Features

- pin submodule to the v5.0.0 release tag ([02493cd3](https://github.com/espressif/esp-protocols/commit/02493cd3))
- mbedTLS 4 / IDF v6 support ([1de9a711](https://github.com/espressif/esp-protocols/commit/1de9a711))

### Bug Fixes

- accumulate fragmented binary receive in client example ([f1d4b543](https://github.com/espressif/esp-protocols/commit/f1d4b543))
- feed the task WDT in the client example service loop ([51240787](https://github.com/espressif/esp-protocols/commit/51240787))
- mutual-auth pytest — request client cert and honour skip-CN check ([7b957f58](https://github.com/espressif/esp-protocols/commit/7b957f58))
- provide if_nametoindex fallback for IDF <= v5.3 ([77317b6e](https://github.com/espressif/esp-protocols/commit/77317b6e))

## [4.3.3~3](https://github.com/espressif/esp-protocols/commits/lws-v4.3.3_3)

### Features

- add LWS_WITH_CUSTOM_HEADERS option and UTC timegm ([8fd4febf](https://github.com/espressif/esp-protocols/commit/8fd4febf))

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
