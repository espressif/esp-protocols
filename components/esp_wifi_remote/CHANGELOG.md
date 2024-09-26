# Changelog

## [0.4.0](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.4.0)

### Features

- Make esp_hosted default RPC library ([1b62adbd](https://github.com/espressif/esp-protocols/commit/1b62adbd))
- Add build test for current IDF examples ([50c113e4](https://github.com/espressif/esp-protocols/commit/50c113e4))
- Support for IDF v5.3 in a separate directory ([bde97203](https://github.com/espressif/esp-protocols/commit/bde97203))
- Support for IDF v5.4 via a separate dir ([e9ac41e1](https://github.com/espressif/esp-protocols/commit/e9ac41e1))
- Add slave selection and peview targets ([345c4577](https://github.com/espressif/esp-protocols/commit/345c4577))

### Bug Fixes

- Fix CMake to use inherent IDF build vars ([c454ec09](https://github.com/espressif/esp-protocols/commit/c454ec09))
- Update per v5.4 espressif/esp-idf@97e42349 ([ff5dac70](https://github.com/espressif/esp-protocols/commit/ff5dac70))
- Fix CI builds to generate configs per slave selection ([8795d164](https://github.com/espressif/esp-protocols/commit/8795d164))
- Depend on esp_hosted only on targets with no WiFi ([7ca5ed1d](https://github.com/espressif/esp-protocols/commit/7ca5ed1d))
- Update per espressif/esp-idf@27f61966 ([2e53b81f](https://github.com/espressif/esp-protocols/commit/2e53b81f))
- Fix checking API compat against reference dir ([1a57a878](https://github.com/espressif/esp-protocols/commit/1a57a878))

## [0.3.0](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.3.0)

### Features

- add esp_wifi_internal_tx_by_ref ([ba35d14e](https://github.com/espressif/esp-protocols/commit/ba35d14e))
- Make wifi_remote depend on esp_hosted ([ac9972aa](https://github.com/espressif/esp-protocols/commit/ac9972aa))

## [0.2.3](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.3)

### Bug Fixes

- Fix server event/command race condtion using eventfd ([732b1d5](https://github.com/espressif/esp-protocols/commit/732b1d5))
- Lock server before marshalling events ([9e13870](https://github.com/espressif/esp-protocols/commit/9e13870))

## [0.2.2](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.2)

### Bug Fixes

- Added more netif options for eppp connection ([24ce867](https://github.com/espressif/esp-protocols/commit/24ce867))
- Do not restrict EPPP config to RSA keys only ([f05c765](https://github.com/espressif/esp-protocols/commit/f05c765), [#570](https://github.com/espressif/esp-protocols/issues/570))

## [0.2.1](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.1)

### Bug Fixes

- Added misc wifi API in eppp impl ([93256d1](https://github.com/espressif/esp-protocols/commit/93256d1))
- Updated eppp dependency not to use fixed version ([3a48c06](https://github.com/espressif/esp-protocols/commit/3a48c06))

## [0.2.0](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.0)

### Features

- Add support for simple eppp based RPC ([fd168d8](https://github.com/espressif/esp-protocols/commit/fd168d8))

### Bug Fixes

- Make services restartable, code cleanup ([6c82ce2](https://github.com/espressif/esp-protocols/commit/6c82ce2))
- Add examples to CI ([d2b7c55](https://github.com/espressif/esp-protocols/commit/d2b7c55))

## [0.1.12](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.1.12)

### Features

- Added generation step for wifi_remote based on IDF ([dfb00358](https://github.com/espressif/esp-protocols/commit/dfb00358))
- Move to esp-protocols ([edc3c2d](https://github.com/espressif/esp-protocols/commit/edc3c2d))
