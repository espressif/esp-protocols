# PPP Connection Example

This example demonstrates how to establish a PPP (Point-to-Point Protocol) connection using the `net_connect` component. PPP creates a simple IP tunnel over a serial connection (UART or USB CDC) to a counterpart device running a PPP server, typically a Linux machine with `pppd` service.

## Overview

The example shows how to:
- Configure and establish a PPP connection over serial (UART or USB CDC)
- Obtain IPv4 and IPv6 addresses from the PPP server
- Retrieve network interface information
- Handle connection lifecycle (connect/disconnect)

## Prerequisites

### Hardware
- ESP32 development board
- Serial connection to Linux host:
  - **UART**: Connect ESP32 UART pins to a USB-to-Serial adapter
  - **USB CDC**: Connect ESP32 via USB (if supported by your board)

### Software
- ESP-IDF v5.0 or higher
- Linux host machine with `pppd` installed:
  ```bash
  # Ubuntu/Debian
  sudo apt-get install ppp

  # Fedora/RHEL
  sudo dnf install ppp
  ```

## Configuration

### Using menuconfig

Run `idf.py menuconfig` and navigate to `Component config → Net Connect Configuration`:

1. **Enable PPP**: Set `connect using Point to Point interface` to `y`
2. **Choose Device**: Select either:
   - `USB` - Use USB CDC (requires SOC_USB_OTG_SUPPORTED)
   - `UART` - Use UART (default pins: TX=4, RX=5)
3. **UART Configuration** (if UART selected):
   - Set TX and RX pin numbers
   - Set baudrate (default: 115200)
4. **IPv6 Support**: Enable `Obtain IPv6 address` if needed
5. **Retry Configuration**: Set maximum connection retries

### Using sdkconfig.defaults

The example includes a `sdkconfig.defaults` file with default configurations. The configuration depends on your target board:

#### Board-Specific Configuration

**For boards with USB OTG support** (ESP32-S3, ESP32-S2, ESP32-P4, ESP32-H4):
- These boards support USB CDC (appears as `/dev/ttyACMx` on Linux)
- **Configuration steps**:
  1. Edit `sdkconfig.defaults` and ensure:
     ```
     CONFIG_NET_CONNECT_PPP_DEVICE_USB=y
     # CONFIG_NET_CONNECT_PPP_DEVICE_UART=y
     ```
  2. Edit `main/idf_component.yml` and ensure the `esp_tinyusb` dependency is **uncommented**:
     ```yaml
     dependencies:
       idf:
         version: '>=5.0'
       net_connect:
         version: '*'
         override_path: ../../../
       espressif/esp_tinyusb: ^2
     ```
  3. Rebuild the project:
     ```bash
     idf.py build
     ```

**For boards without USB OTG support** (ESP32, ESP32-C3, ESP32-C6, ESP32-C61):
- These boards require UART mode with a USB-to-Serial adapter (appears as `/dev/ttyUSBx` on Linux)
- **Configuration steps**:
  1. Edit `sdkconfig.defaults` and ensure:
     ```
     # CONFIG_NET_CONNECT_PPP_DEVICE_USB=y
     CONFIG_NET_CONNECT_PPP_DEVICE_UART=y
     ```
  2. Edit `main/idf_component.yml` and ensure the `esp_tinyusb` dependency is **commented out**:
     ```yaml
     dependencies:
       idf:
         version: '>=5.0'
       net_connect:
         version: '*'
         override_path: ../../../
       # espressif/esp_tinyusb: ^2  # Not needed for UART mode
     ```
  3. Configure UART pins in `sdkconfig.defaults` (if different from defaults):
     ```
     CONFIG_NET_CONNECT_UART_TX_PIN=4
     CONFIG_NET_CONNECT_UART_RX_PIN=5
     CONFIG_NET_CONNECT_UART_BAUDRATE=115200
     ```
  4. Rebuild the project:
     ```bash
     idf.py build
     ```

> **Important**: The `esp_tinyusb` dependency must match your device selection:
> - **USB CDC mode**: `esp_tinyusb` dependency must be **uncommented** in `idf_component.yml`
> - **UART mode**: `esp_tinyusb` dependency must be **commented out** in `idf_component.yml`

## PPP Server Setup

### Step 1: Identify Serial Device

Connect your ESP32 board and identify the serial device name:

```bash
# List USB devices
ls /dev/ttyACM*

# List UART devices (USB-to-Serial adapters)
ls /dev/ttyUSB*

# Check dmesg for device information
dmesg | tail
```

Common device names:
- `/dev/ttyACMx` - USB CDC devices
- `/dev/ttyUSBx` - USB-to-Serial adapters

### Step 2: Run pppd Server

Run the `pppd` server with appropriate parameters:

```bash
sudo pppd /dev/ttyACM0 115200 \
  192.168.11.1:192.168.11.2 \
  ms-dns 8.8.8.8 \
  modem local noauth debug nocrtscts nodetach +ipv6
```

**Parameter Explanation**:
- `/dev/ttyACM0` - Serial device name (update to match your device)
- `115200` - Baudrate (must match ESP32 configuration)
- `192.168.11.1:192.168.11.2` - Server IP:Client IP addresses
- `ms-dns 8.8.8.8` - DNS server address
- `modem` - Use modem control lines
- `local` - Don't use modem control lines for flow control
- `noauth` - Disable authentication
- `debug` - Enable debug output
- `nocrtscts` - Disable hardware flow control
- `nodetach` - Run in foreground (for debugging)
- `+ipv6` - Enable IPv6 (only if `CONFIG_NET_CONNECT_IPV6=y`)

**For IPv4 only** (if IPv6 is disabled):
```bash
sudo pppd /dev/ttyACM0 115200 \
  192.168.11.1:192.168.11.2 \
  ms-dns 8.8.8.8 \
  modem local noauth debug nocrtscts nodetach
```

### Step 3: Verify Connection

After running `pppd`, you should see:
- PPP negotiation messages in the terminal
- A new network interface `ppp0` created
- IP addresses assigned

Check the interface:
```bash
ip addr show ppp0
```

You should see the server IP address (e.g., `192.168.11.1`) assigned to `ppp0`.

## Building and Running

### Build the Example

```bash
cd examples/ppp_connect_example
idf.py build
```

### Flash and Monitor

```bash
idf.py flash monitor
```

**Important**: Make sure the `pppd` server is running on your Linux host **before** flashing the ESP32, or start it immediately after the ESP32 boots.

### Expected Output

```
I (301) ppp_connect_example: Starting PPP connection example...
I (301) ppp_connect_example: This example demonstrates PPP connection over serial
I (311) ppp_connect_example: PPP device: USB CDC
I (341) ppp_connect_example: Network stack initialized
I (361) ppp_connect_example: IPv6 support enabled
I (361) ppp_connect_example: Connecting to PPP server...
I (361) ppp_connect_example: Make sure pppd is running on the host machine
I (371) net_connect: Initializing PPP interface...
I (371) example_connect_ppp: Start example_connect.
I (381) example_connect_ppp: USB initialization
W (381) tusb_desc: No Device descriptor provided, using default.
W (391) tusb_desc: No FullSpeed configuration descriptor provided, using default.
W (391) tusb_desc: No String descriptors provided, using default.
I (401) tusb_desc:
┌─────────────────────────────────┐
│  USB Device Descriptor Summary  │
├───────────────────┬─────────────┤
│bDeviceClass       │ 239         │
├───────────────────┼─────────────┤
│bDeviceSubClass    │ 2           │
├───────────────────┼─────────────┤
│bDeviceProtocol    │ 1           │
├───────────────────┼─────────────┤
│bMaxPacketSize0    │ 64          │
├───────────────────┼─────────────┤
│idVendor           │ 0x303a      │
├───────────────────┼─────────────┤
│idProduct          │ 0x4001      │
├───────────────────┼─────────────┤
│bcdDevice          │ 0x100       │
├───────────────────┼─────────────┤
│iManufacturer      │ 0x1         │
├───────────────────┼─────────────┤
│iProduct           │ 0x2         │
├───────────────────┼─────────────┤
│iSerialNumber      │ 0x3         │
├───────────────────┼─────────────┤
│bNumConfigurations │ 0x1         │
└───────────────────┴─────────────┘
I (571) TinyUSB: TinyUSB Driver installed
I (571) example_connect_ppp: Waiting for IP address
I (9501) example_connect_ppp: Line state changed on channel 0
I (12601) esp-netif_lwip-ppp: net_connect_netif_ppp: Connected
I (12601) example_connect_ppp: Got IPv6 event: Interface "net_connect_netif_ppp" address: fe80:0000:0000:0000:bcda:18a9:27fd:39e4, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (12611) esp-netif_lwip-ppp: net_connect_netif_ppp: Connected
I (12611) example_connect_ppp: Got IPv4 event: Interface "net_connect_netif_ppp" address: 192.168.11.2
I (12611) example_connect_ppp: Connected!
I (12631) example_connect_ppp: Main DNS server : 8.8.8.8
I (12641) net_connect: PPP interface initialized successfully
I (12641) net_connect: Connected to net_connect_netif_ppp
I (12651) net_connect: - IPv4 address: 192.168.11.2,
I (12651) net_connect: - IPv6 address: fe80:0000:0000:0000:bcda:18a9:27fd:39e4, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (12661) ppp_connect_example: PPP connection established successfully!
I (12671) ppp_connect_example: PPP netif retrieved: net_connect_netif_ppp
I (12671) ppp_connect_example: IPv4 Address: 192.168.11.2
I (12681) ppp_connect_example: IPv4 Netmask: 255.255.255.255
I (12681) ppp_connect_example: IPv4 Gateway: 192.168.11.1
I (12691) ppp_connect_example: DNS Server: 8.8.8.8
I (12691) ppp_connect_example: IPv6 Address: fe80:0000:0000:0000:bcda:18a9:27fd:39e4
I (12701) ppp_connect_example: PPP connection active. Waiting 30 seconds...
I (12711) ppp_connect_example: You can now test network connectivity from the ESP32
I (42721) ppp_connect_example: Disconnecting PPP connection...
I (42721) net_connect: Disconnecting network interfaces...
I (42721) net_connect: Deinitializing PPP interface...
I (42721) net_connect: PPP interface deinitialized
I (42721) net_connect: All network interfaces disconnected
I (42731) ppp_connect_example: PPP connection disconnected successfully
I (42741) ppp_connect_example: Example finished
I (42741) main_task: Returned from app_main()
```

## Troubleshooting

### Connection Fails Immediately

**Problem**: ESP32 fails to connect to PPP server.

**Solutions**:
1. Verify `pppd` is running on the host:
   ```bash
   ps aux | grep pppd
   ```
2. Check serial device name matches:
   ```bash
   ls -l /dev/ttyACM* /dev/ttyUSB*
   ```
3. Verify baudrate matches in both ESP32 config and pppd command
4. Check serial connection (cables, pins)
5. Review `pppd` debug output for negotiation errors

### Connection Established but No IP Address

**Problem**: PPP connection is established but IP address is not obtained.

**Solutions**:
1. Check `pppd` output for IPCP negotiation errors
2. Verify IP address range in pppd command matches network configuration
3. Check for IP address conflicts
4. Review ESP32 logs for IP event errors

### USB CDC Not Working

**Problem**: USB CDC device not recognized or not working.

**Solutions**:
1. Verify your board supports USB OTG (ESP32-S3, ESP32-S2, ESP32-P4, ESP32-H4)
   - If your board doesn't support USB OTG, use UART mode instead (see [Board-Specific Configuration](#board-specific-configuration))
2. Verify `CONFIG_NET_CONNECT_PPP_DEVICE_USB=y` is set in your `sdkconfig.defaults`
3. Ensure the `esp_tinyusb` component is **uncommented** in your application's `idf_component.yml`:
   ```yaml
   dependencies:
     espressif/esp_tinyusb: ^2
   ```
   Or run: `idf.py add-dependency espressif/esp_tinyusb^2`
4. Check USB cable supports data transfer (not charge-only)
5. Verify board supports USB OTG (check `SOC_USB_OTG_SUPPORTED` capability)
6. Check Linux USB permissions (may need to add user to `dialout` group)
7. Verify device appears as `/dev/ttyACMx` (not `/dev/ttyUSBx`) on Linux

### UART Connection Issues

**Problem**: UART-based PPP connection fails.

**Solutions**:
1. Verify your board configuration:
   - For boards without USB OTG (ESP32, ESP32-C3, ESP32-C6, etc.), ensure `CONFIG_NET_CONNECT_PPP_DEVICE_UART=y` is set
   - Ensure `esp_tinyusb` dependency is **commented out** in `idf_component.yml` (see [Board-Specific Configuration](#board-specific-configuration))
2. Verify TX/RX pins are correctly connected (crossed: ESP32 TX → Host RX, ESP32 RX → Host TX)
3. Check baudrate matches on both sides (default: 115200)
4. Verify UART pins in `sdkconfig.defaults` match hardware connections (default: TX=4, RX=5)
5. Check for pin conflicts with other peripherals
6. Verify USB-to-Serial adapter is working (device should appear as `/dev/ttyUSBx` on Linux)
7. Ensure USB-to-Serial adapter drivers are installed on your host system

## Additional Resources

- [net_connect Component Documentation](../../README.md)
- [ESP-IDF Networking Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/index.html)
- [lwIP PPP Documentation](https://www.nongnu.org/lwip/2_1_x/ppp.html)
- [Linux pppd Manual](https://linux.die.net/man/8/pppd)

## Notes

- This example is intended for testing and development purposes
- For production use with cellular modems, consider using [esp_modem](https://components.espressif.com/component/espressif/esp_modem) component
- PPP over serial has limited bandwidth compared to WiFi/Ethernet
- The example disconnects after 30 seconds for demonstration; modify the code to keep connection alive as needed
