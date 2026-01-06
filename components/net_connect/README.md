# net_connect

This component implements the most common connection methods for ESP32 boards. It provides WiFi, Ethernet, Thread, and PPP connection functionality for network-enabled applications.

## How to use this component

Choose the preferred interface (WiFi, Ethernet, Thread, PPPoS) to connect to the network and configure the interface.

It is possible to enable multiple interfaces simultaneously making the connection phase to block until all the chosen interfaces acquire IP addresses.
It is also possible to disable all interfaces, skipping the connection phase altogether.

### WiFi

Choose WiFi connection method (for chipsets that support it) and configure basic WiFi connection properties:
* WiFi SSID
* WiFI password
* Maximum connection retry (connection would be aborted if it doesn't succeed after specified number of retries)
* WiFi scan method (including RSSI and authorization mode threshold)



### Ethernet

Choose Ethernet connection if your board supports it. The most common settings is using Espressif Ethernet Kit, which is also the recommended HW for this selection. You can also select an SPI ethernet device (if your chipset doesn't support internal EMAC or if you prefer). It is also possible to use OpenCores Ethernet MAC if you're running the example under QEMU.

### Thread

Choose Thread connection if your board supports IEEE802.15.4 native radio or works with [OpenThread RCP](../../openthread/ot_rcp/README.md). You can configure the Thread network at menuconfig '->Components->OpenThread->Thread Core Features->Thread Operational Dataset'.

If the Thread end-device joins a Thread network with a Thread Border Router that has the NAT64 feature enabled, the end-device can access the Internet with the standard DNS APIs after configuring the following properties:
* Enable DNS64 client ('->Components->OpenThread->Thread Core Features->Enable DNS64 client')
* Enable custom DNS external resolve Hook ('->Components->LWIP->Hooks->DNS external resolve Hook->Custom implementation')

### PPP

Point to point connection method creates a simple IP tunnel to the counterpart device (running PPP server), typically a Linux machine with pppd service. We currently support only PPP over Serial (using UART or USB CDC). This is useful for simple testing of networking layers, but with some additional configuration on the server side, we could simulate standard model of internet connectivity. The PPP server could be also represented by a cellular modem device with pre-configured connectivity and already switched to PPP mode (this setup is not very flexible though, so we suggest using a standard modem library implementing commands and modes, e.g. [esp_modem](https://components.espressif.com/component/espressif/esp_modem) ).

#### Board-Specific Configuration

The PPP device selection and dependency configuration depend on your target board:

**For boards with USB OTG support** (ESP32-S3, ESP32-S2, ESP32-P4, ESP32-H4):
- Use USB CDC mode (`CONFIG_NET_CONNECT_PPP_DEVICE_USB=y`)
- **Required**: Add `esp_tinyusb` component as a dependency in your project's `idf_component.yml`:
  ```yaml
  dependencies:
    espressif/esp_tinyusb: ^2
  ```
  Or run: `idf.py add-dependency espressif/esp_tinyusb^2`
- The device will appear as `/dev/ttyACMx` on Linux

**For boards without USB OTG support** (ESP32, ESP32-C3, ESP32-C6, ESP32-C61):
- Use UART mode (`CONFIG_NET_CONNECT_PPP_DEVICE_UART=y`)
- **Required**: Do **not** include `esp_tinyusb` dependency (comment it out or remove it from `idf_component.yml`)
- Connect via USB-to-Serial adapter (appears as `/dev/ttyUSBx` on Linux)
- Configure UART pins (default: TX=4, RX=5) and baudrate (default: 115200)

> [!Note]
> The `esp_tinyusb` dependency must match your device selection:
> - **USB CDC mode**: `esp_tinyusb` dependency **must be present** in `idf_component.yml`
> - **UART mode**: `esp_tinyusb` dependency **must be commented out or removed** from `idf_component.yml`
>
> This is needed because ESP-IDF's build system resolves dependencies from components to applications (not the other way around), so the component needs `esp_tinyusb` declared in your project's dependencies to access `tinyusb.h` during compilation when using USB CDC mode. This step is necessary to keep the `net_connect` component simple and dependency free.

For detailed PPP setup instructions, server configuration, and examples, see the [PPP Connection Example README](examples/ppp_connect_example/README.md).
