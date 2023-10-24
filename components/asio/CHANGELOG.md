# Changelog

## [1.28.0~1](https://github.com/espressif/esp-protocols/commits/asio-v1.28.0_1)

- Updated only metadata, previous tag wasn't created.

## [1.28.0~0](https://github.com/espressif/esp-protocols/commits/asio-1.28.0~0)

### Features

- Updates asio to 1.28 ([b310abe](https://github.com/espressif/esp-protocols/commit/b310abe))

### Bug Fixes

- Makes the examples to override path ([842b2b2](https://github.com/espressif/esp-protocols/commit/842b2b2))
- Removes esp_exception and make all examples to use exceptions ([c1c9350](https://github.com/espressif/esp-protocols/commit/c1c9350))
- removed Wno-format flag and fixed formatting warnings ([c48e442](https://github.com/espressif/esp-protocols/commit/c48e442))
- added idf_component.yml for examples ([d273e10](https://github.com/espressif/esp-protocols/commit/d273e10))
- Reintroduce missing CHANGELOGs ([200cbb3](https://github.com/espressif/esp-protocols/commit/200cbb3), [#235](https://github.com/espressif/esp-protocols/issues/235))

### Updated

- docs(common): updated component and example links ([f48d9b2](https://github.com/espressif/esp-protocols/commit/f48d9b2))
- docs(common): improving documentation ([ca3fce0](https://github.com/espressif/esp-protocols/commit/ca3fce0))
- Add homepage URL and License to all components ([ef3f0ee](https://github.com/espressif/esp-protocols/commit/ef3f0ee))
- Added badges with version of components to the respective README files ([e4c8a59](https://github.com/espressif/esp-protocols/commit/e4c8a59))
- CI: Fix ASIO example test ([6e2bb51](https://github.com/espressif/esp-protocols/commit/6e2bb51))
- Examples: using pytest.ini from top level directory ([aee016d](https://github.com/espressif/esp-protocols/commit/aee016d))
- CI: fixing the files to be complient with pre-commit hooks ([945bd17](https://github.com/espressif/esp-protocols/commit/945bd17))

## [1.14.1~3](https://github.com/espressif/esp-protocols/commits/f148c98)

### Updated

- ASIO: Updated package version to "1.14.1~3" ([f148c98](https://github.com/espressif/esp-protocols/commit/f148c98))
- ASIO: Example tests integration ([5193ebc](https://github.com/espressif/esp-protocols/commit/5193ebc))


## [1.0.2](https://github.com/espressif/esp-protocols/commits/57afa38)

### Updated

- Bump asio/mdns/esp_websocket_client versions ([57afa38](https://github.com/espressif/esp-protocols/commit/57afa38))
- ignore format warnings ([d66f9dc](https://github.com/espressif/esp-protocols/commit/d66f9dc))
- asio: Fix exampels build ([3662c14](https://github.com/espressif/esp-protocols/commit/3662c14))


## [1.0.1](https://github.com/espressif/esp-protocols/commits/055f051)

### Updated

- ASIO: Initial version based on IDF 5.0 with history ([055f051](https://github.com/espressif/esp-protocols/commit/055f051))
- asio: Disable concepts support       Fix example for compatibility with C++20 ([ac7bf46](https://github.com/espressif/esp-protocols/commit/ac7bf46), [IDF@9dba047](https://github.com/espressif/esp-idf/commit/9dba0476a01cd80d76e21706ad350009606b877e))
- lwip: Update socket API to include port-version of sockets/netdb ([057a5d2](https://github.com/espressif/esp-protocols/commit/057a5d2), [IDF@53c009e](https://github.com/espressif/esp-idf/commit/53c009e62631bae569fa849c6b6c9e70a10b3afe))
- esp_netif/lwip: Implement basic support for vanilla-lwip (2.1.3-REL) ([63bff63](https://github.com/espressif/esp-protocols/commit/63bff63), [IDF@5b471a1](https://github.com/espressif/esp-idf/commit/5b471a18489b056f65fe8dcbb2c992d27909ebc9))
- mbedtls: Remove certs.c and certs.h from port directory ([e3c4391](https://github.com/espressif/esp-protocols/commit/e3c4391), [IDF@f31d8dd](https://github.com/espressif/esp-idf/commit/f31d8dd2955be0fe949340dcf3b043ec6daf4378))
- mbedtls-3 update: 1) Fix build issue in mbedtls 2) skip the public headers check in IDF 3)Update Kconfig Macros 4)Remove deprecated config options 5) Update the sha API according to new nomenclature 6) Update mbedtls_rsa_init usage 7) Include mbedtls/build_info.h instead of mbedtls/config.h 8) Dont include check_config.h 9) Add additional error message in esp_blufi_api.h ([9813818](https://github.com/espressif/esp-protocols/commit/9813818), [IDF@4512253](https://github.com/espressif/esp-idf/commit/45122533e0bca5d538585e22308f14b74c33e474))
- asio: Use internal ssl context and engine impl ([f605fdd](https://github.com/espressif/esp-protocols/commit/f605fdd), [IDF@d823106](https://github.com/espressif/esp-idf/commit/d823106aa6b24b8bdcc30373513c8688c61438c4))
- Build & config: Remove leftover files from the unsupported "make" build system ([abbc8d9](https://github.com/espressif/esp-protocols/commit/abbc8d9), [IDF@766aa57](https://github.com/espressif/esp-idf/commit/766aa5708443099f3f033b739cda0e1de101cca6))
- openssl: Add deprecation warning to ssl.h ([a029774](https://github.com/espressif/esp-protocols/commit/a029774), [IDF@cfc0018](https://github.com/espressif/esp-idf/commit/cfc001870c5e0afed7b42b6bf8c326eae053fe96))
- asio coap: If LWIP IPV6 is disabled, automatically don't build asio & coap ([cc0f2b3](https://github.com/espressif/esp-protocols/commit/cc0f2b3), [IDF@e305f29](https://github.com/espressif/esp-idf/commit/e305f2938278c2a39e75c21a3ed59d8f4d4e62fa))
- asio: update copyright notice ([47d57a5](https://github.com/espressif/esp-protocols/commit/47d57a5), [IDF@2d0895e](https://github.com/espressif/esp-idf/commit/2d0895e9a98bc7846d0ac7321f2b86b47346bf21))
- Whitespace: Automated whitespace fixes (large commit) ([622a360](https://github.com/espressif/esp-protocols/commit/622a360), [IDF@66fb5a2](https://github.com/espressif/esp-idf/commit/66fb5a29bbdc2482d67c52e6f66b303378c9b789))
- cmake: Apply cmakelint fixes ([4358c3c](https://github.com/espressif/esp-protocols/commit/4358c3c), [IDF@e82eac4](https://github.com/espressif/esp-idf/commit/e82eac4354b8b4111697656f3acce7450eeff366))
- asio: option to use wolfSSL as TLS stack for ASIO ([c05558b](https://github.com/espressif/esp-protocols/commit/c05558b), [IDF@1c8171c](https://github.com/espressif/esp-idf/commit/1c8171c3e8d5a67e47dc8d6abac27ad2989470c3))
- asio: Basic SSL/TLS support in asio port for ESP platform ([dab1230](https://github.com/espressif/esp-protocols/commit/dab1230), [IDF@9459c0d](https://github.com/espressif/esp-idf/commit/9459c0dd432fdd0fccb49ea65bb5c72d1849e1ba))
- asio: updated ASIO port to use latest asio and esp-idf features ([9190917](https://github.com/espressif/esp-protocols/commit/9190917), [IDF@13d603e](https://github.com/espressif/esp-idf/commit/13d603e486624380d49f2e89614b10425c208d14))
- components: use new component registration api ([9e83b1e](https://github.com/espressif/esp-protocols/commit/9e83b1e), [IDF@9eccd7c](https://github.com/espressif/esp-idf/commit/9eccd7c0826d6cc2e9de59304d1e5f76c0063ccf))
- Rename Kconfig options (root) ([3b49d1f](https://github.com/espressif/esp-protocols/commit/3b49d1f), [IDF@c5000c8](https://github.com/espressif/esp-idf/commit/c5000c83d250896fffbddd7a3991384ea0fc286d))
- cmake: make main a component again ([57672d5](https://github.com/espressif/esp-protocols/commit/57672d5), [IDF@d9939ce](https://github.com/espressif/esp-idf/commit/d9939cedd9b44d63dc148354c3a0a139b9c7113d))
- asio: initial idf port of asio library without ssl ([5472d5c](https://github.com/espressif/esp-protocols/commit/5472d5c), [IDF@1ef13c5](https://github.com/espressif/esp-idf/commit/1ef13c524c484e9fb62e6c0b11482c30c5383728))
