# esp_wifi_remote

[![Component Registry](https://components.espressif.com/components/espressif/esp_wifi_remote/badge.svg)](https://components.espressif.com/components/espressif/esp_wifi_remote)

This component wraps the public API of `esp_wifi` and provides a set of the same function calls namespaces with `esp_wifi_remote` prefix that translate to RPC calls to another target device (called `slave` device) which executes the appropriate `esp_wifi` APIs.

This component is heavily dependent on a specific version of the `esp_wifi` component, as that's why most of its headers, sources and configuration files are pre-generated based on the actual version of `esp_wifi`.

This component doesn't implement the RPC calls by itself, but uses their dependencies for the functionality. Currently, only `esp_hosted` is supported to provide the RPC functionality of `esp_wifi_remote`.


## Dependencies on `esp_wifi`

Public API needs to correspond exactly to the `esp_wifi` API. Some of the internal types depend on the actual wifi target, as well as some default configuration values. Therefore it's easier to maintain consistency between this component and the exact version of `esp_wifi` automatically in CI:

We extract function prototypes from `esp_wifi.h` and use them to generate `esp_wifi_remote` function declarations (and forwarding the definitions to the underlying RPC component -- `esp_hosted`)
We process the local `esp_wifi_types_native.h` and replace `CONFIG_IDF_TARGET` to `CONFIG_SLAVE_IDF_TARGET` and `CONFIG_SOC_WIFI_...` to `CONFIG_SLAVE_...`.
Similarly we process `esp_wifi`'s Kconfig, so the dependencies are on the slave target and slave SOC capabilities.

Please check the [README.md](./scripts/README.md) for more details on the generation step and testing consistency.
