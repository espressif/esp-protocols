# Plan: Make Number of Channels Configurable in eppp_link

## 1. Update Kconfig ✅ (completed)
- Add a new boolean configuration option (e.g., `CONFIG_EPPP_LINK_CHANNELS_SUPPORT`) to enable channel support in the `components/eppp_link/Kconfig` file.
- Add a new integer configuration option (e.g., `CONFIG_EPPP_LINK_NR_OF_CHANNELS`) that is only visible when the boolean option is enabled.
- Set reasonable defaults (e.g., boolean disabled by default, integer default of 2) and provide help text for both options.
- Ensure both options are properly documented in the menu with appropriate dependencies.
- **Note:** Channel support options are not available when Ethernet transport is selected (`!EPPP_LINK_DEVICE_ETH`).

## 2. Update Code to Use Config Value ✅ (completed)
- Replace all hardcoded uses of `#define NR_OF_CHANNELS 2` in `eppp_transport.h` with a macro that uses the Kconfig value (e.g., `#define NR_OF_CHANNELS CONFIG_EPPP_LINK_NR_OF_CHANNELS`).
- Update all `.c` files that reference `NR_OF_CHANNELS` to ensure they use the new configurable value.
- Add `#include "sdkconfig.h"` where needed to access the Kconfig value.
- Use `#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT` for all channel-related code:
    - The typedef for `eppp_channel_fn_t` and the `eppp_add_channels` API in `eppp_link.h` are now conditionally compiled.
    - The `channel_tx` and `channel_rx` members in the handle struct are also conditionally compiled.
- Ensure that all channel-related APIs and struct members are only available when channel support is enabled in Kconfig.

## 3. Update Documentation
- Update `components/eppp_link/README.md` to document the new configuration option and its effect.
- Mention the new option in the "Configuration" section, with usage notes.

## 4. Update Example Projects
- Optionally, update example `sdkconfig.defaults` files in `examples/host` and `examples/slave` to show how to set the number of channels.
- Add a note in example READMEs if relevant.

## 5. Test and Validate
- Build and run the examples with different values for the number of channels to ensure the configuration is respected and works as intended.
