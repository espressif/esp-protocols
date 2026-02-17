| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# Ethernet Station + Wi-Fi AP Example

## Overview

This example initializes **two network interfaces simultaneously**: an **Ethernet interface** operating as a **Station (STA)** with DHCP client, and a **Wi-Fi interface** operating as an **Access Point (AP)** with a DHCP server. The Ethernet Station connects to an external network via DHCP, while the Wi-Fi AP provides network access to connected devices via DHCP (e.g. in the `192.168.5.x` range). This hybrid configuration enables the ESP to bridge between a wired upstream connection and a local wireless network.

The example uses the `ethernet_init` component from [esp-eth-drivers](https://github.com/espressif/esp-eth-drivers) to initialize the Ethernet driver based on menuconfig (internal/SPI PHY, etc.) and creates two `esp_netif` instances: one for Ethernet Station (configured with `ESP_NETIF_DEFAULT_ETH` for DHCP client mode) and one for Wi-Fi AP (configured with `esp_netif_create_default_wifi_ap()` with static IP and DHCP server).

### Key Features

- **Ethernet Station with DHCP Client**: Automatically obtains IP address from Ethernet network
- **Wi-Fi AP with DHCP Server**: Automatically assigns IP addresses to devices connected via Wi-Fi
- **Network Bridging**: Bridges traffic between Ethernet and Wi-Fi interfaces
- **NAPT Support**: Optional Network Address Port Translation for routing between interfaces
- **Configurable IP Pool**: Set the range of IP addresses to assign via DHCP (default: `192.168.5.2` - `192.168.5.100`)
- **DHCP Lease Time**: Configurable lease duration for assigned IP addresses
- **DNS Support**: Optional DNS server configuration for DHCP clients, can use Ethernet Station's DNS servers
- **Event-Driven Architecture**: Uses ESP event system to handle network events for both interfaces

## How to use example

### Configure the project

You can configure the project using `idf.py menuconfig` or by using the provided `sdkconfig.defaults` file which contains default configuration values.

#### Ethernet Configuration

Configure the Ethernet PHY/SPI settings under "Ethernet" in menuconfig (see the [ethernet_init](https://github.com/espressif/esp-eth-drivers/tree/master/ethernet_init) component documentation). The example depends on the managed component `espressif/ethernet_init` (see `main/idf_component.yml`).

#### Example Configuration

Under "Example Configuration" menu, you can configure:

**Wi-Fi AP Configuration:**
- **WiFi AP SSID**: Wi-Fi network SSID for the Access Point (default: `"myssid-ext"`)
- **WiFi AP Password**: Wi-Fi network password (default: `"mypassword"`)
- **WiFi AP Channel**: Wi-Fi channel for the AP (default: 1)
- **WiFi AP IP Address**: Static IP address for the ESP device (default: `192.168.5.1`)
- **WiFi AP Netmask**: Subnet mask for the network (default: `255.255.255.0`)
- **WiFi AP Gateway**: Default gateway IP (default: `192.168.5.1`)
- **DHCP Lease Time**: Duration of IP address leases in units of `LWIP_DHCPS_LEASE_UNIT` (default: 120, which equals 7200 seconds or 2 hours)
- **DHCP IP Pool Start Address**: First IP address in the DHCP pool (default: `192.168.5.2`)
- **DHCP IP Pool End Address**: Last IP address in the DHCP pool (default: `192.168.5.100`)
- **Enable DNS in DHCP Offers**: Enable DNS server information in DHCP offers (default: enabled)
- **Primary DNS Server**: Primary DNS server IP (default: `192.168.5.1`, can be updated from Ethernet Station DNS)
- **Backup DNS Server**: Backup DNS server IP (default: `8.8.8.8`)

**NAPT Configuration:**
- Enable IP forwarding and NAPT in menuconfig under "Component config" → "LWIP" → "Enable IPv4 forwarding" and "Enable IPv4 NAPT"

### Build, Flash, and Run

```
idf.py -p PORT build flash monitor
```

Replace PORT with your serial port.

**Prerequisites:**
- Connect an Ethernet cable to the ESP's Ethernet port (connected to a network with DHCP server)
- Ensure devices can connect to the Wi-Fi AP with the configured SSID and password

The example initializes the Ethernet Station first, then starts the Wi-Fi AP. It waits for the Ethernet Station to obtain an IP address before completing initialization. If DNS is enabled, the Wi-Fi AP's DNS servers will be updated with the Ethernet Station's DNS servers automatically.

## Example Output

```
I (1234) eth_sta_wifi_ap: Ethernet Station initialized, waiting for IP address...
I (1234) eth_sta_wifi_ap: Wi-Fi AP initialized. AP IP: 192.168.5.1, netmask: 255.255.255.0
I (1234) eth_sta_wifi_ap: Connect a device to the Wi-Fi AP to get an IP via DHCP
I (1234) eth_sta_wifi_ap: Wi-Fi AP Got IP Address
I (1234) eth_sta_wifi_ap: ~~~~~~~~~~~
I (1234) eth_sta_wifi_ap: APIP:192.168.5.1
I (1234) eth_sta_wifi_ap: APMASK:255.255.255.0
I (1234) eth_sta_wifi_ap: APGW:192.168.5.1
I (1244) eth_sta_wifi_ap: DHCP_DNS_MAIN:192.168.5.1
I (1244) eth_sta_wifi_ap: DHCP_DNS_BACKUP:8.8.8.8
I (1254) eth_sta_wifi_ap: ~~~~~~~~~~~
I (1254) eth_sta_wifi_ap: Wi-Fi AP started. SSID:myssid-ext password:mypassword channel:1
I (1254) eth_sta_wifi_ap: Wi-Fi Event: base=WIFI_EVENT, id=0
I (1254) eth_sta_wifi_ap: Wi-Fi AP started
I (1264) ethernet_init: Ethernet(IP101[23,18]) Link Up
I (1264) ethernet_init: Ethernet(IP101[23,18]) HW Addr 58:bf:25:e0:41:03
I (1274) eth_sta_wifi_ap: Ethernet Link Up
I (1274) eth_sta_wifi_ap: Ethernet HW Addr 58:bf:25:e0:41:03
I (1284) eth_sta_wifi_ap: Ethernet Got IP Address
I (1284) eth_sta_wifi_ap: Event: base=IP_EVENT, id=4
I (1284) eth_sta_wifi_ap: ~~~~~~~~~~~
I (1284) eth_sta_wifi_ap: ETHIP:192.168.1.100
I (1284) eth_sta_wifi_ap: ETHMASK:255.255.255.0
I (1284) eth_sta_wifi_ap: ETHGW:192.168.1.1
I (1294) eth_sta_wifi_ap: DHCP_DNS_MAIN:192.168.1.1
I (1294) eth_sta_wifi_ap: DHCP_DNS_BACKUP:8.8.8.8
I (1304) eth_sta_wifi_ap: ~~~~~~~~~~~
I (1304) eth_sta_wifi_ap: Updated Wi-Fi AP DNS with Ethernet Station DNS: 192.168.1.1
I (1304) eth_sta_wifi_ap: NAPT enabled on Wi-Fi AP
I (1404) eth_sta_wifi_ap: Wi-Fi Event: base=WIFI_EVENT, id=2
I (1414) eth_sta_wifi_ap: station xx:xx:xx:xx:xx:xx join, AID=1
I (1424) eth_sta_wifi_ap: Wi-Fi AP assigned IP to client: 192.168.5.2
```

After initialization:
- The Ethernet Station connects to the Ethernet network and receives an IP address via DHCP
- Devices connected to the Wi-Fi AP will receive IPs in the `192.168.5.0/24` range via DHCP
- The ESP can be reached at `192.168.5.1` on the Wi-Fi AP network
- If NAPT is enabled, traffic from Wi-Fi AP clients can be routed through the Ethernet connection
- DNS servers from the Ethernet Station are automatically used for Wi-Fi AP DHCP clients (if DNS is enabled)

## Use Cases

- **Ethernet-to-Wi-Fi Bridges**: Connect wireless devices to a wired Ethernet network
- **Network Extenders**: Extend Ethernet networks to wireless devices
- **IoT Gateways**: Provide wireless access with Ethernet backhaul
- **Home Automation Hubs**: Connect wireless sensors to Ethernet networks
- **Network Isolation**: Create a separate Wi-Fi network while maintaining internet connectivity via Ethernet

## Troubleshooting

- **Ethernet Link Not Up**: Ensure the Ethernet cable is properly connected and the PHY configuration matches your hardware. Verify that the Ethernet network has a DHCP server running.
- **Client Not Getting IP**: Check that the DHCP server started successfully (look for "DHCP server started" in the logs)
- **Wi-Fi AP Not Starting**: Check that the SSID and password are correctly configured in menuconfig
- **No IP Address Received on Ethernet**: Verify that the Ethernet network has a DHCP server running. Check network connectivity
- **IP Pool Exhausted**: If you have more than 99 devices, increase the DHCP IP pool range in menuconfig
- **DNS Issues**: If DNS is enabled but not working, verify the DNS server addresses are correct. The example automatically updates Wi-Fi AP DNS with Ethernet Station DNS when available
- **NAPT Not Working**: Ensure IP forwarding and NAPT are enabled in menuconfig (`CONFIG_LWIP_IP_FORWARD=y` and `CONFIG_LWIP_IPV4_NAPT=y`)
- **Hardware Issues**: For hardware and driver issues, refer to ESP-IDF Ethernet and Wi-Fi documentation and the upper level [README](../README.md)

## Configuration Notes

- The maximum DHCP pool size is 100 addresses (`DHCPS_MAX_LEASE`)
- DHCP lease time is specified in units of `LWIP_DHCPS_LEASE_UNIT` (default 60 seconds)
- The Wi-Fi AP always uses a static IP address (configured via menuconfig)
- DNS server information is optional and can be disabled in menuconfig
- When DNS is enabled, the Wi-Fi AP's primary DNS is automatically updated with the Ethernet Station's DNS server after Ethernet connects
- The example waits for Ethernet Station to obtain an IP address before completing initialization
- NAPT is optional and must be enabled in menuconfig for routing between interfaces
- Both interfaces operate independently - Ethernet Station provides upstream connectivity while Wi-Fi AP provides local network access
