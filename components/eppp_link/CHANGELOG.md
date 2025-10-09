# Changelog

## [1.1.3](https://github.com/espressif/esp-protocols/commits/eppp-v1.1.3)

### Bug Fixes

- Fix test dependency issue on driver ([1ace92c2](https://github.com/espressif/esp-protocols/commit/1ace92c2))
- Fix tun netif to (optionally) return errors ([7a6cf0f9](https://github.com/espressif/esp-protocols/commit/7a6cf0f9))

## [1.1.2](https://github.com/espressif/esp-protocols/commits/eppp-v1.1.2)

### Bug Fixes

- Update uart driver deps per IDF > v5.3 ([92e14607](https://github.com/espressif/esp-protocols/commit/92e14607))

## [1.1.1](https://github.com/espressif/esp-protocols/commits/eppp-v1.1.1)

### Bug Fixes

- Fix getting context for channel API ([94563cdc](https://github.com/espressif/esp-protocols/commit/94563cdc))
- Cover more combinations in build tests ([e0b8de8f](https://github.com/espressif/esp-protocols/commit/e0b8de8f))

## [1.1.0](https://github.com/espressif/esp-protocols/commits/eppp-v1.1.0)

### Features

- Add support for UART flow control ([cd57f1bb](https://github.com/espressif/esp-protocols/commit/cd57f1bb), [#870](https://github.com/espressif/esp-protocols/issues/870))

### Bug Fixes

- Fix SPI transport to allow already init GPIO ISR ([497ee2d6](https://github.com/espressif/esp-protocols/commit/497ee2d6), [#868](https://github.com/espressif/esp-protocols/issues/868))
- Fix stack-overflow in ping task for TUN netif ([b2568a3d](https://github.com/espressif/esp-protocols/commit/b2568a3d), [#867](https://github.com/espressif/esp-protocols/issues/867))

### Updated

- ci(common): Update test component dir for IDFv6.0 ([18418c83](https://github.com/espressif/esp-protocols/commit/18418c83))

## [1.0.1](https://github.com/espressif/esp-protocols/commits/eppp-v1.0.1)

### Bug Fixes

- Support for IPv4-only mode ([653328ba](https://github.com/espressif/esp-protocols/commit/653328ba), [#864](https://github.com/espressif/esp-protocols/issues/864))

## [1.0.0](https://github.com/espressif/esp-protocols/commits/eppp-v1.0.0)

### Features

- Add support for custom channels ([4ee9360f](https://github.com/espressif/esp-protocols/commit/4ee9360f))

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
