| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# Ethernet AP + Wi-Fi AP Example

## Overview

This example initializes **two network interfaces simultaneously**: an **Ethernet interface** operating as an **Access Point (AP)** with a DHCP server, and a **Wi-Fi interface** also operating as an **Access Point (AP)** with a DHCP server. Both interfaces provide network access to connected devices via DHCP simultaneously. The Ethernet AP provides network access to wired devices (e.g. in the `192.168.5.x` range), while the Wi-Fi AP provides network access to wireless devices (e.g. in the `192.168.4.x` range). This dual AP configuration enables the ESP to act as a multi-interface router, providing both wired and wireless network access.

The example uses the `ethernet_init` component from [esp-eth-drivers](https://github.com/espressif/esp-eth-drivers) to initialize the Ethernet driver based on menuconfig (internal/SPI PHY, etc.) and creates two `esp_netif` instances: one for Ethernet AP (configured with `ESP_NETIF_DHCP_SERVER` and a static AP IP) and one for Wi-Fi AP (configured with `esp_netif_create_default_wifi_ap()`).

### Key Features

- **Ethernet AP with DHCP Server**: Automatically assigns IP addresses to devices connected via Ethernet
- **Wi-Fi AP with DHCP Server**: Automatically assigns IP addresses to devices connected via Wi-Fi
- **Dual Network Access**: Provides both wired and wireless network access simultaneously
- **NAPT Support**: Optional Network Address Port Translation for routing between interfaces
- **Configurable IP Pools**: Set the range of IP addresses to assign via DHCP for both interfaces (Ethernet: `192.168.5.2` - `192.168.5.100`, Wi-Fi: `192.168.4.2` - `192.168.4.100`)
- **DHCP Lease Time**: Configurable lease duration for assigned IP addresses on both interfaces
- **DNS Support**: Optional DNS server configuration for DHCP clients on both interfaces
- **Event-Driven Architecture**: Uses ESP event system to handle network events for both interfaces
- **Client Management**: Tracks and logs client connections/disconnections for both interfaces

## How to use example

### Configure the project

You can configure the project using `idf.py menuconfig` or by using the provided `sdkconfig.defaults` file which contains default configuration values.

#### Ethernet Configuration

Configure the Ethernet PHY/SPI settings under "Ethernet" in menuconfig (see the [ethernet_init](https://github.com/espressif/esp-eth-drivers/tree/master/ethernet_init) component documentation). The example depends on the managed component `espressif/ethernet_init` (see `main/idf_component.yml`).

#### Example Configuration

Under "Example Configuration" menu, you can configure:

**Wi-Fi AP Configuration:**
- **WiFi AP SSID**: Wi-Fi network SSID for the access point (default: `"myssid-ext"`)
- **WiFi AP Password**: Wi-Fi network password (default: `"mypassword"`)
- **WiFi AP Channel**: Wi-Fi channel for the access point (default: `1`, range: 1-13)
- **WiFi AP IP Address**: Static IP address for the Wi-Fi AP interface (default: `192.168.4.1`)
- **WiFi AP Netmask**: Subnet mask for the Wi-Fi network (default: `255.255.255.0`)
- **WiFi AP Gateway**: Default gateway IP (default: `192.168.4.1`)
- **DHCP Lease Time**: Duration of IP address leases in units of `LWIP_DHCPS_LEASE_UNIT` (default: 120, which equals 7200 seconds or 2 hours)
- **DHCP IP Pool Start Address**: First IP address in the DHCP pool (default: `192.168.4.2`)
- **DHCP IP Pool End Address**: Last IP address in the DHCP pool (default: `192.168.4.100`)
- **Enable DNS in DHCP Offers**: Enable DNS server information in DHCP offers (default: enabled)
- **Primary DNS Server**: Primary DNS server IP (default: `192.168.4.1`)
- **Backup DNS Server**: Backup DNS server IP (default: `8.8.8.8`)

**Ethernet AP Configuration:**
- **Ethernet AP IP Address**: Static IP address for the ESP device (default: `192.168.5.1`)
- **Ethernet AP Netmask**: Subnet mask for the network (default: `255.255.255.0`)
- **Ethernet AP Gateway**: Default gateway IP (default: `192.168.5.1`)
- **DHCP Lease Time**: Duration of IP address leases in units of `LWIP_DHCPS_LEASE_UNIT` (default: 120, which equals 7200 seconds or 2 hours)
- **DHCP IP Pool Start Address**: First IP address in the DHCP pool (default: `192.168.5.2`)
- **DHCP IP Pool End Address**: Last IP address in the DHCP pool (default: `192.168.5.100`)
- **Enable DNS in DHCP Offers**: Enable DNS server information in DHCP offers (default: enabled)
- **Primary DNS Server**: Primary DNS server IP (default: `192.168.5.1`)
- **Backup DNS Server**: Backup DNS server IP (default: `8.8.8.8`)

**NAPT Configuration:**
- Enable IP forwarding and NAPT in menuconfig under "Component config" → "LWIP" → "Enable IPv4 forwarding" and "Enable IPv4 NAPT"

### Build, Flash, and Run

```
idf.py -p PORT build flash monitor
```

Replace PORT with your serial port.

**Prerequisites:**
- Connect an Ethernet cable to the ESP's Ethernet port (devices connecting will receive IPs via DHCP)
- Ensure Wi-Fi devices can connect to the configured SSID and password

The example initializes both the Ethernet AP and Wi-Fi AP simultaneously. Both interfaces operate independently and provide network access to their respective clients via DHCP.

## Example Output

```
I (589) eth_ap_wifi_ap: Ethernet AP netif configured. AP IP: 192.168.5.1, netmask: 255.255.255.0
I (589) esp_eth.netif.netif_glue: 24:d7:eb:bb:f2:1b
I (589) esp_eth.netif.netif_glue: ethernet attached to netif
I (3799) ethernet_init: Ethernet(IP101[23,18]) Started
I (3799) esp_netif_lwip: DHCP server started on interface ETH_AP with IP: 192.168.5.1
I (3809) ethernet_init: Ethernet(IP101[23,18]) Link Up
I (3809) ethernet_init: Ethernet(IP101[23,18]) HW Addr 24:d7:eb:bb:f2:1b
I (3819) eth_ap_wifi_ap: Ethernet Link Up
I (3819) eth_ap_wifi_ap: Ethernet HW Addr 24:d7:eb:bb:f2:1b
I (3799) eth_ap_wifi_ap: Ethernet Started
I (3799) eth_ap_wifi_ap: Ethernet Got IP Address
I (3799) eth_ap_wifi_ap: ~~~~~~~~~~~
I (3829) eth_ap_wifi_ap: IP:192.168.5.1
I (3829) eth_ap_wifi_ap: MASK:255.255.255.0
I (3839) eth_ap_wifi_ap: GW:192.168.5.1
I (3839) eth_ap_wifi_ap: DHCP_DNS_MAIN:192.168.5.1
I (3839) eth_ap_wifi_ap: DHCP_DNS_BACKUP:8.8.8.8
I (3849) eth_ap_wifi_ap: ~~~~~~~~~~~
I (3849) eth_ap_wifi_ap: Ethernet AP started. Connect a device to the Ethernet port to get an IP via DHCP
I (3859) eth_ap_wifi_ap: Wi-Fi AP initialized.
I (3879) wifi:wifi driver task: 3ffc41c8, prio:23, stack:6656, core=0
I (3889) wifi:wifi firmware version: 0f5722b
I (3889) wifi:wifi certification version: v7.0
I (3889) wifi:config NVS flash: enabled
I (3899) wifi:config nano formatting: disabled
I (3899) wifi:Init data frame dynamic rx buffer num: 32
I (3899) wifi:Init static rx mgmt buffer num: 5
I (3909) wifi:Init management short buffer num: 32
I (3909) wifi:Init dynamic tx buffer num: 32
I (3919) wifi:Init static rx buffer size: 1600
I (3919) wifi:Init static rx buffer num: 10
I (3929) wifi:Init dynamic rx buffer num: 32
I (3929) wifi_init: rx ba win: 6
I (3929) wifi_init: accept mbox: 6
I (3939) wifi_init: tcpip mbox: 32
I (3939) wifi_init: udp mbox: 6
I (3939) wifi_init: tcp mbox: 6
I (3939) wifi_init: tcp tx win: 5760
I (3949) wifi_init: tcp rx win: 5760
I (3949) wifi_init: tcp mss: 1440
I (3949) wifi_init: WiFi IRAM OP enabled
I (3959) wifi_init: WiFi RX IRAM OP enabled
I (3959) phy_init: phy_version 4863,a3a4459,Oct 28 2025,14:30:06
I (4039) wifi:mode : softAP (24:d7:eb:bb:f2:19)
I (4049) wifi:Total power save buffer number: 16
I (4049) wifi:Init max length of beacon: 752/752
I (4049) wifi:Init max length of beacon: 752/752
I (4049) eth_ap_wifi_ap: Wi-Fi Event: base=WIFI_EVENT, id=12
I (4059) eth_ap_wifi_ap: Wi-Fi AP started
I (4059) eth_ap_wifi_ap: Wi-Fi AP started. SSID:myssid-ext password:mypassword channel:1
I (4059) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
I (4079) eth_ap_wifi_ap: Wi-Fi AP Got IP Address
I (4079) eth_ap_wifi_ap: ~~~~~~~~~~~
I (4079) eth_ap_wifi_ap: IP:192.168.4.1
I (4089) eth_ap_wifi_ap: MASK:255.255.255.0
I (4089) eth_ap_wifi_ap: GW:192.168.4.1
I (4089) eth_ap_wifi_ap: DHCP_DNS_MAIN:192.168.4.1
I (4099) eth_ap_wifi_ap: DHCP_DNS_BACKUP:8.8.8.8
I (4099) eth_ap_wifi_ap: ~~~~~~~~~~~
I (4109) eth_ap_wifi_ap: Wifi AP started. Connect a device to the Wi-Fi AP to get an IP via DHCP
I (4119) eth_ap_wifi_ap: NAPT enabled on Ethernet AP
W (4119) esp_netif_lwip: napt disabled on esp_netif:0x3ffc1a9c
I (4129) eth_ap_wifi_ap: NAPT enabled on Wi-Fi AP
I (4129) main_task: Returned from app_main()
```

After initialization:
- Devices connected to the Ethernet port will receive IPs in the `192.168.5.0/24` range via DHCP
- The ESP can be reached at `192.168.5.1` on the Ethernet network
- Devices connected to the Wi-Fi AP will receive IPs in the `192.168.4.0/24` range via DHCP
- The ESP can be reached at `192.168.4.1` on the Wi-Fi network
- If NAPT is enabled, traffic can be routed between the Ethernet and Wi-Fi interfaces
- Both interfaces operate independently and can accept client connections simultaneously

## Use Cases

- **Dual-Band Network Access Points**: Provide both wired and wireless connectivity simultaneously
- **Network Bridges**: Bridge between wired and wireless networks
- **IoT Gateways**: Provide multiple access methods for IoT devices
- **Network Testing and Development**: Create isolated test networks with both wired and wireless access
- **Multi-Interface Routers**: Act as a router providing both Ethernet and Wi-Fi access for home or office networks
- **Network Extenders**: Extend network access through both wired and wireless interfaces

## Troubleshooting

- **Ethernet Link Not Up**: Ensure the Ethernet cable is properly connected and the PHY configuration matches your hardware
- **Client Not Getting IP on Ethernet**: Check that the DHCP server started successfully (look for "DHCP server started" in the logs)
- **Client Not Getting IP on Wi-Fi**: Check that the Wi-Fi AP started successfully (look for "Wi-Fi AP started" in the logs)
- **Wi-Fi AP Not Starting**: Verify that the SSID and password are configured correctly in menuconfig. Check that the channel is valid (1-13)
- **IP Pool Exhausted**: If you have more than 99 devices on either interface, increase the DHCP IP pool range in menuconfig
- **DNS Issues**: If DNS is enabled but not working, verify the DNS server addresses are correct for each interface
- **NAPT Not Working**: Ensure IP forwarding and NAPT are enabled in menuconfig (`CONFIG_LWIP_IP_FORWARD=y` and `CONFIG_LWIP_IPV4_NAPT=y`)
- **Hardware Issues**: For hardware and driver issues, refer to ESP-IDF Ethernet and Wi-Fi documentation and the upper level [README](../README.md)

## Configuration Notes

- The maximum DHCP pool size is 100 addresses (`DHCPS_MAX_LEASE`) for each interface
- DHCP lease time is specified in units of `LWIP_DHCPS_LEASE_UNIT` (default 60 seconds)
- Both Ethernet AP and Wi-Fi AP use static IP addresses (configured via menuconfig)
- DNS server information is optional and can be disabled in menuconfig for each interface independently
- The Ethernet AP uses subnet `192.168.5.0/24` and Wi-Fi AP uses subnet `192.168.4.0/24` by default to avoid conflicts
- NAPT is optional and must be enabled in menuconfig for routing between interfaces
- Both interfaces operate independently - Ethernet AP provides wired network access while Wi-Fi AP provides wireless network access
- Each interface can independently accept client connections and assign IP addresses via DHCP
