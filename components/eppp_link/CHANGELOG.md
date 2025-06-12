# Changelog

## [0.3.1](https://github.com/espressif/esp-protocols/commits/eppp-v0.3.1)

### Bug Fixes

- Fix NETIF_PPP_STATUS link issue if PPP disabled in lwip ([077ea0bb](https://github.com/espressif/esp-protocols/commit/077ea0bb))

## [0.3.0](https://github.com/espressif/esp-protocols/commits/eppp-v0.3.0)

### Features

- Add support for TUN interface ([2ff150c3](https://github.com/espressif/esp-protocols/commit/2ff150c3))
- Add support for transport via Ethernet link ([a21ce883](https://github.com/espressif/esp-protocols/commit/a21ce883))

## [0.2.0](https://github.com/espressif/esp-protocols/commits/eppp-v0.2.0)

### Features

- Add support for SDIO transport ([085dd790](https://github.com/espressif/esp-protocols/commit/085dd790))

### Bug Fixes

- Fixed strict prototype API decl issue in SDIO ([eb09e426](https://github.com/espressif/esp-protocols/commit/eb09e426))
- Fix SIDO host to check/clear interrupts atomically ([402176c9](https://github.com/espressif/esp-protocols/commit/402176c9))

## [0.1.1](https://github.com/espressif/esp-protocols/commits/eppp-v0.1.1)

### Bug Fixes

- Make some common netif options configurable ([7829e8f](https://github.com/espressif/esp-protocols/commit/7829e8f))

## [0.1.0](https://github.com/espressif/esp-protocols/commits/eppp-v0.1.0)

### Features

- Added CI job to build examples and tests ([7eefcf0](https://github.com/espressif/esp-protocols/commit/7eefcf0))

### Bug Fixes

- Fixed to select PPP LWIP opts which are OFF by default ([16be2f9](https://github.com/espressif/esp-protocols/commit/16be2f9))
- Example to use iperf component from the registry ([bd6b66d](https://github.com/espressif/esp-protocols/commit/bd6b66d))
- Fixed defalt config designated init issue in C++ ([8bd4712](https://github.com/espressif/esp-protocols/commit/8bd4712))

## [0.0.1](https://github.com/espressif/esp-protocols/commits/eppp-v0.0.1)

### Features

- Added CI job to build examples and tests ([8686977](https://github.com/espressif/esp-protocols/commit/8686977))
- Added support for SPI transport ([18f8452](https://github.com/espressif/esp-protocols/commit/18f8452))
- Added support for UART transport ([ad27414](https://github.com/espressif/esp-protocols/commit/ad27414))
- Introduced ESP-PPP-Link component ([a761039](https://github.com/espressif/esp-protocols/commit/a761039))
