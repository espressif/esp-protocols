# Changelog

## [0.3.0](https://github.com/espressif/esp-protocols/commits/sock_utils-v0.3.0)

### Features

- Add IPv6 to getifaddrs() and getnameinfo() ([db49d9e7](https://github.com/espressif/esp-protocols/commit/db49d9e7))

### Bug Fixes

- Fix getifaddrs() overflow of the interface name ([f993bd8f](https://github.com/espressif/esp-protocols/commit/f993bd8f))
- Make poll.h provide the poll API ([c1de6211](https://github.com/espressif/esp-protocols/commit/c1de6211))
- Fix getifaddrs() leak on allocation failure ([f916397d](https://github.com/espressif/esp-protocols/commit/f916397d))
- Fix host test build failure with ubuntu-26.04 ([7fb59671](https://github.com/espressif/esp-protocols/commit/7fb59671))
- Fix potential CI race in sockutils tests ([c613a1dd](https://github.com/espressif/esp-protocols/commit/c613a1dd))
- Force vfs_lib linkage for host/tests ([3edaa261](https://github.com/espressif/esp-protocols/commit/3edaa261))
- Pass empty cmake for linux target ([b52d2ecb](https://github.com/espressif/esp-protocols/commit/b52d2ecb))
- Use minimal build for examples ([726c41f8](https://github.com/espressif/esp-protocols/commit/726c41f8))

## [0.2.2](https://github.com/espressif/esp-protocols/commits/sock_utils-v0.2.2)

### Bug Fixes

- Fix gai_strerror() impl to return const string ([f12a2056](https://github.com/espressif/esp-protocols/commit/f12a2056))

## [0.2.1](https://github.com/espressif/esp-protocols/commits/sock_utils-v0.2.1)

### Bug Fixes

- Fix potential macro colission including standard headers ([ade9448c](https://github.com/espressif/esp-protocols/commit/ade9448c))

## [0.2.0](https://github.com/espressif/esp-protocols/commits/sock_utils-v0.2.0)

### Features

- Declare socketpair and gai_strerror via standard headers ([b090a3cb](https://github.com/espressif/esp-protocols/commit/b090a3cb))
- Add support for gethostname() ([f7c0b756](https://github.com/espressif/esp-protocols/commit/f7c0b756))

## [0.1.0](https://github.com/espressif/esp-protocols/commits/sock_utils-v0.1.0)

### Features

- Add initial support for socket helpers ([31f57ad0](https://github.com/espressif/esp-protocols/commit/31f57ad0))
