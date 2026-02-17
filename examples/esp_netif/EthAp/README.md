| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# Ethernet AP Example

## Overview

This example initializes a **single** Ethernet interface and runs a **DHCP server** on it, turning the ESP into an **Ethernet Access Point**. Devices that connect to the Ethernet port receive an IP address automatically via DHCP (e.g. in the `192.168.5.x` range). The ESP uses a static IP on that interface (default `192.168.5.1`).

The example uses the `ethernet_init` component from [esp-eth-drivers](https://github.com/espressif/esp-eth-drivers) to initialize the Ethernet driver based on menuconfig (internal/SPI PHY, etc.) and a single `esp_netif` instance configured with `ESP_NETIF_DHCP_SERVER` and a static AP IP.

### Key Features

- **DHCP Server**: Automatically assigns IP addresses to connected devices
- **Configurable IP Pool**: Set the range of IP addresses to assign (default: `192.168.5.2` - `192.168.5.100`)
- **DHCP Lease Time**: Configurable lease duration for assigned IP addresses
- **DNS Support**: Optional DNS server configuration for DHCP clients
- **Static AP IP**: ESP device uses a static IP address (default: `192.168.5.1`)

## How to use example

### Configure the project

You can configure the project using `idf.py menuconfig` or by using the provided `sdkconfig.defaults` file which contains default configuration values.

#### Ethernet Configuration

Configure the Ethernet PHY/SPI settings under "Ethernet" in menuconfig (see the [ethernet_init](https://github.com/espressif/esp-eth-drivers/tree/master/ethernet_init) component documentation). The example depends on the managed component `espressif/ethernet_init` (see `main/idf_component.yml`).

#### Example Configuration

Under "Example Configuration" menu, you can configure:

- **Ethernet AP IP Address**: Static IP address for the ESP device (default: `192.168.5.1`)
- **Ethernet AP Netmask**: Subnet mask for the network (default: `255.255.255.0`)
- **Ethernet AP Gateway**: Default gateway IP (default: `192.168.5.1`)
- **DHCP Lease Time**: Duration of IP address leases in units of `LWIP_DHCPS_LEASE_UNIT` (default: 120, which equals 7200 seconds or 2 hours)
- **DHCP IP Pool Start Address**: First IP address in the DHCP pool (default: `192.168.5.2`)
- **DHCP IP Pool End Address**: Last IP address in the DHCP pool (default: `192.168.5.100`)
- **Enable DNS in DHCP Offers**: Enable DNS server information in DHCP offers (default: enabled)
- **Primary DNS Server**: Primary DNS server IP (default: `192.168.5.1`)
- **Backup DNS Server**: Backup DNS server IP (default: `8.8.8.8`)

### Build, Flash, and Run

```
idf.py -p PORT build flash monitor
```

Replace PORT with your serial port. Connect a PC or another device to the ESP's Ethernet port; the device should obtain an IP via DHCP (e.g. `192.168.5.2`). You can then ping the ESP at `192.168.5.1`.

## Example Output

```
I (3635) ethernet_init: Ethernet(IP101[23,18]) Started
I (3635) eth_ap: Ethernet Started
I (3635) eth_ap: Ethernet AP initialized. AP IP: 192.168.5.1, netmask: 255.255.255.0
I (3635) esp_netif_lwip: DHCP server started on interface ETH_AP with IP: 192.168.5.1
I (3645) ethernet_init: Ethernet(IP101[23,18]) Link Up
I (3645) ethernet_init: Ethernet(IP101[23,18]) HW Addr 58:bf:25:e0:41:03
I (3655) eth_ap: Ethernet Link Up
I (3655) eth_ap: Ethernet HW Addr 58:bf:25:e0:41:03
I (3665) eth_ap: Connect a device to the Ethernet port to get an IP via DHCP
I (3665) main_task: Returned from app_main()
I (4645) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.5.2
I (25635) ethernet_init: Ethernet(IP101[23,18]) Link Down
I (25635) eth_ap: Ethernet Link Down
I (27635) ethernet_init: Ethernet(IP101[23,18]) Link Up
I (27635) ethernet_init: Ethernet(IP101[23,18]) HW Addr 58:bf:25:e0:41:03
I (27635) eth_ap: Ethernet Link Up
I (27635) eth_ap: Ethernet HW Addr 58:bf:25:e0:41:03
I (28215) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.5.2
I (3628765) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.5.2
I (7228765) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.5.2
```

After connecting a device, it will receive an IP in the `192.168.5.0/24` range and can reach the ESP at `192.168.5.1`.

## Troubleshooting

- **Ethernet Link Not Up**: Ensure the Ethernet cable is properly connected and the PHY configuration matches your hardware
- **Client Not Getting IP**: Check that the DHCP server started successfully (look for "DHCP server started" in the logs)
- **IP Pool Exhausted**: If you have more than 99 devices, increase the DHCP IP pool range in menuconfig
- **DNS Issues**: If DNS is enabled but not working, verify the DNS server addresses are correct
- **Hardware Issues**: For hardware and driver issues, refer to ESP-IDF Ethernet documentation and the upper level [README](../README.md)

## Configuration Notes

- The maximum DHCP pool size is 100 addresses (`DHCPS_MAX_LEASE`)
- DHCP lease time is specified in units of `LWIP_DHCPS_LEASE_UNIT` (default 60 seconds)
- The ESP device always uses a static IP address (configured via menuconfig)
- DNS server information is optional and can be disabled in menuconfig
