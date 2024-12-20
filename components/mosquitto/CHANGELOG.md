# Changelog

## [2.0.20~1](https://github.com/espressif/esp-protocols/commits/mosq-v2.0.20_1)

### Bug Fixes

- Use sock_utils instead of func stubs ([3cd0ed37](https://github.com/espressif/esp-protocols/commit/3cd0ed37))
- Update API docs adding on-message callback ([5dcc3330](https://github.com/espressif/esp-protocols/commit/5dcc3330))

## [2.0.20](https://github.com/espressif/esp-protocols/commits/mosq-v2.0.20)

### Features

- Upgrade to mosquitto v2.0.20 ([3b2c614d](https://github.com/espressif/esp-protocols/commit/3b2c614d))
- Add support for on-message callback ([cdeab8f5](https://github.com/espressif/esp-protocols/commit/cdeab8f5))
- Add example with two brokers synced on P2P ([d57b8c5b](https://github.com/espressif/esp-protocols/commit/d57b8c5b))

### Bug Fixes

- Fix dependency issues moving esp-tls to public deps ([6cce87e4](https://github.com/espressif/esp-protocols/commit/6cce87e4))

## [2.0.28~0](https://github.com/espressif/esp-protocols/commits/mosq-v2.0.28_0)

### Warning

Incorrect version number! This version published under `2.0.28~0` is based on upstream v2.0.18

### Features

- Added support for TLS transport using ESP-TLS ([1af4bbe1](https://github.com/espressif/esp-protocols/commit/1af4bbe1))
- Add API docs, memory consideration and tests ([a20c0c9d](https://github.com/espressif/esp-protocols/commit/a20c0c9d))
- Add target tests with localhost broker-client ([5c850cda](https://github.com/espressif/esp-protocols/commit/5c850cda))
- Initial moquitto v2.0.18 port (TCP only) ([de4531e8](https://github.com/espressif/esp-protocols/commit/de4531e8))

### Bug Fixes

- Fix clean compilation addressing _GNU_SOURCE redefined ([e2392c36](https://github.com/espressif/esp-protocols/commit/e2392c36))

### Updated

- docs(mosq): Prepare mosquitto component for publishing ([c2c4bf83](https://github.com/espressif/esp-protocols/commit/c2c4bf83))
