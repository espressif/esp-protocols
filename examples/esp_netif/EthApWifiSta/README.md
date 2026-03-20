| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# Ethernet AP + Wi-Fi STA Example

## Overview

This example initializes **two network interfaces simultaneously**: an **Ethernet interface** operating as an **Access Point (AP)** with a DHCP server, and a **Wi-Fi interface** operating as a **Station (STA)**. The Ethernet AP provides network access to connected devices via DHCP (e.g. in the `192.168.5.x` range), while the Wi-Fi STA connects to an external access point for upstream connectivity. This hybrid configuration enables the ESP to bridge between a wired local network and a wireless upstream connection.

The example uses the `ethernet_init` component from [esp-eth-drivers](https://github.com/espressif/esp-eth-drivers) to initialize the Ethernet driver based on menuconfig (internal/SPI PHY, etc.) and creates two `esp_netif` instances: one for Ethernet AP (configured with `ESP_NETIF_DHCP_SERVER` and a static AP IP) and one for Wi-Fi STA (configured with `esp_netif_create_default_wifi_sta()`).

### Key Features

- **Ethernet AP with DHCP Server**: Automatically assigns IP addresses to devices connected via Ethernet
- **Wi-Fi STA**: Connects to an external Wi-Fi access point for internet connectivity
- **Network Bridging**: Bridges traffic between Ethernet and Wi-Fi interfaces
- **NAPT Support**: Optional Network Address Port Translation for routing between interfaces
- **Configurable IP Pool**: Set the range of IP addresses to assign via DHCP (default: `192.168.5.2` - `192.168.5.100`)
- **DHCP Lease Time**: Configurable lease duration for assigned IP addresses
- **DNS Support**: Optional DNS server configuration for DHCP clients, can use Wi-Fi STA's DNS servers
- **Event-Driven Architecture**: Uses ESP event system to handle network events for both interfaces

## How to use example

### Configure the project

You can configure the project using `idf.py menuconfig` or by using the provided `sdkconfig.defaults` file which contains default configuration values.

#### Ethernet Configuration

Configure the Ethernet PHY/SPI settings under "Ethernet" in menuconfig (see the [ethernet_init](https://github.com/espressif/esp-eth-drivers/tree/master/ethernet_init) component documentation). The example depends on the managed component `espressif/ethernet_init` (see `main/idf_component.yml`).

#### Example Configuration

Under "Example Configuration" menu, you can configure:

**Wi-Fi Configuration:**
- **WiFi SSID**: Wi-Fi network SSID to connect to (default: `"myssid"`)
- **WiFi Password**: Wi-Fi network password (default: `"mypassword"`)

**Ethernet AP Configuration:**
- **Ethernet AP IP Address**: Static IP address for the ESP device (default: `192.168.5.1`)
- **Ethernet AP Netmask**: Subnet mask for the network (default: `255.255.255.0`)
- **Ethernet AP Gateway**: Default gateway IP (default: `192.168.5.1`)
- **DHCP Lease Time**: Duration of IP address leases in units of `LWIP_DHCPS_LEASE_UNIT` (default: 120, which equals 7200 seconds or 2 hours)
- **DHCP IP Pool Start Address**: First IP address in the DHCP pool (default: `192.168.5.2`)
- **DHCP IP Pool End Address**: Last IP address in the DHCP pool (default: `192.168.5.100`)
- **Enable DNS in DHCP Offers**: Enable DNS server information in DHCP offers (default: enabled)
- **Primary DNS Server**: Primary DNS server IP (default: `192.168.5.1`, can be updated from Wi-Fi STA DNS)
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
- Ensure a Wi-Fi access point is available with the configured SSID and password

The example initializes the Ethernet AP first, then connects the Wi-Fi STA. It waits for the Wi-Fi STA to obtain an IP address before completing initialization. If DNS is enabled, the Ethernet AP's DNS servers will be updated with the Wi-Fi STA's DNS servers automatically.

## Example Output

```
I (1234) eth_ap_wifi_sta: Ethernet AP initialized. AP IP: 192.168.5.1, netmask: 255.255.255.0
I (1234) eth_ap_wifi_sta: Connect a device to the Ethernet port to get an IP via DHCP
I (1234) esp_netif_lwip: DHCP server started on interface ETH_AP with IP: 192.168.5.1
I (1244) ethernet_init: Ethernet(IP101[23,18]) Link Up
I (1244) ethernet_init: Ethernet(IP101[23,18]) HW Addr 58:bf:25:e0:41:03
I (1254) eth_ap_wifi_sta: Ethernet Link Up
I (1254) eth_ap_wifi_sta: Ethernet HW Addr 58:bf:25:e0:41:03
I (1254) eth_ap_wifi_sta: Ethernet Got IP Address
I (1254) eth_ap_wifi_sta: Event: base=IP_EVENT, id=4
I (1254) eth_ap_wifi_sta: ~~~~~~~~~~~
I (1254) eth_ap_wifi_sta: ETHIP:192.168.5.1
I (1254) eth_ap_wifi_sta: ETHMASK:255.255.255.0
I (1254) eth_ap_wifi_sta: ETHGW:192.168.5.1
I (1264) eth_ap_wifi_sta: DHCP_DNS_MAIN:192.168.5.1
I (1264) eth_ap_wifi_sta: DHCP_DNS_BACKUP:8.8.8.8
I (1274) eth_ap_wifi_sta: ~~~~~~~~~~~
I (1274) eth_ap_wifi_sta: Wi-Fi STA initialized. SSID:myssid password:mypassword
I (1274) eth_ap_wifi_sta: Wi-Fi Event: base=WIFI_EVENT, id=2
I (1284) eth_ap_wifi_sta: Wi-Fi STA started
I (1424) wifi:connected with myssid, aid = 1, channel 6, BW20, bssid = xx:xx:xx:xx:xx:xx
I (1424) wifi:security: WPA2-PSK, phy: bgn, rssi: -45
I (1434) eth_ap_wifi_sta: Wi-Fi Event: base=WIFI_EVENT, id=4
I (1434) eth_ap_wifi_sta: Wi-Fi STA connected
I (2534) eth_ap_wifi_sta: Wi-Fi Got IP Address
I (2534) eth_ap_wifi_sta: Event: base=IP_EVENT, id=0
I (2534) eth_ap_wifi_sta: ~~~~~~~~~~~
I (2534) eth_ap_wifi_sta: STAIP:192.168.1.100
I (2534) eth_ap_wifi_sta: STAMASK:255.255.255.0
I (2534) eth_ap_wifi_sta: STAGW:192.168.1.1
I (2544) eth_ap_wifi_sta: DHCP_DNS_MAIN:192.168.1.1
I (2544) eth_ap_wifi_sta: DHCP_DNS_BACKUP:8.8.8.8
I (2554) eth_ap_wifi_sta: ~~~~~~~~~~~
I (2554) eth_ap_wifi_sta: Updated Ethernet AP DNS with WiFi STA DNS: 192.168.1.1
I (2554) eth_ap_wifi_sta: NAPT enabled on Ethernet AP
I (2644) esp_netif_lwip: DHCP server assigned IP to a client, IP is: 192.168.5.2
```

After initialization:
- Devices connected to the Ethernet port will receive IPs in the `192.168.5.0/24` range via DHCP
- The ESP can be reached at `192.168.5.1` on the Ethernet network
- The Wi-Fi STA connects to the external access point and obtains an IP address
- If NAPT is enabled, traffic from Ethernet devices can be routed through the Wi-Fi connection
- DNS servers from the Wi-Fi STA are automatically used for Ethernet AP DHCP clients (if DNS is enabled)

## Use Cases

- **Wi-Fi-to-Ethernet Bridges**: Connect wired devices to a Wi-Fi network
- **Network Extenders**: Extend Wi-Fi networks to wired devices
- **IoT Gateways**: Provide wired access with wireless backhaul
- **Home Automation Hubs**: Connect wired sensors to Wi-Fi networks
- **Network Isolation**: Create a separate Ethernet network while maintaining internet connectivity via Wi-Fi

## Troubleshooting

- **Ethernet Link Not Up**: Ensure the Ethernet cable is properly connected and the PHY configuration matches your hardware
- **Client Not Getting IP**: Check that the DHCP server started successfully (look for "DHCP server started" in the logs)
- **Wi-Fi Not Connecting**: Check that the SSID and password are correct in menuconfig. Ensure the Wi-Fi access point is within range and operational
- **No IP Address Received on Wi-Fi**: Verify that the Wi-Fi network has a DHCP server running. Check network connectivity
- **IP Pool Exhausted**: If you have more than 99 devices, increase the DHCP IP pool range in menuconfig
- **DNS Issues**: If DNS is enabled but not working, verify the DNS server addresses are correct. The example automatically updates Ethernet AP DNS with Wi-Fi STA DNS when available
- **NAPT Not Working**: Ensure IP forwarding and NAPT are enabled in menuconfig (`CONFIG_LWIP_IP_FORWARD=y` and `CONFIG_LWIP_IPV4_NAPT=y`)
- **Hardware Issues**: For hardware and driver issues, refer to ESP-IDF Ethernet and Wi-Fi documentation and the upper level [README](../README.md)

## Configuration Notes

- The maximum DHCP pool size is 100 addresses (`DHCPS_MAX_LEASE`)
- DHCP lease time is specified in units of `LWIP_DHCPS_LEASE_UNIT` (default 60 seconds)
- The Ethernet AP always uses a static IP address (configured via menuconfig)
- DNS server information is optional and can be disabled in menuconfig
- When DNS is enabled, the Ethernet AP's primary DNS is automatically updated with the Wi-Fi STA's DNS server after Wi-Fi connects
- The example waits for Wi-Fi STA to obtain an IP address before completing initialization
- NAPT is optional and must be enabled in menuconfig for routing between interfaces
- Both interfaces operate independently - Ethernet AP provides local network access while Wi-Fi STA provides upstream connectivity
