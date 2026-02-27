| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- |

# Ethernet STA + Wi-Fi STA Example

## Overview

This example initializes **two network interfaces simultaneously**: an **Ethernet interface** and a **Wi-Fi STA interface**. Both interfaces operate as **DHCP clients**, connecting to their respective networks and obtaining IP addresses automatically. The ESP device connects to a Wi-Fi access point and also connects to an Ethernet network, enabling dual connectivity scenarios.

The example uses the `ethernet_init` component from [esp-eth-drivers](https://github.com/espressif/esp-eth-drivers) to initialize the Ethernet driver based on menuconfig (internal/SPI PHY, etc.) and creates two `esp_netif` instances: one for Ethernet (configured with `ESP_NETIF_DEFAULT_ETH`) and one for Wi-Fi STA (configured with `esp_netif_create_default_wifi_sta()`).

### Key Features

- **Dual Interface Support**: Simultaneously operates Ethernet and Wi-Fi STA interfaces
- **DHCP Client**: Both interfaces obtain IP addresses automatically via DHCP
- **Independent Connectivity**: Each interface connects to its respective network independently
- **Event-Driven Architecture**: Uses ESP event system to handle network events for both interfaces
- **DNS Support**: Receives and displays DNS server information from DHCP for both interfaces

## How to use example

### Configure the project

You can configure the project using `idf.py menuconfig` or by editing the source code directly.

#### Ethernet Configuration

Configure the Ethernet PHY/SPI settings under "Ethernet" in menuconfig (see the [ethernet_init](https://github.com/espressif/esp-eth-drivers/tree/master/ethernet_init) component documentation). The example depends on the managed component `espressif/ethernet_init` (see `main/idf_component.yml`).

#### Wi-Fi Configuration

Configure the Wi-Fi credentials by editing the following defines in `main/EthStaWifiSta.c`:

- **EXAMPLE_ESP_WIFI_SSID**: Wi-Fi network SSID (default: `"myssid"`)
- **EXAMPLE_ESP_WIFI_PASS**: Wi-Fi network password (default: `"mypassword"`)

Alternatively, you can configure these via menuconfig if you add Kconfig options to the example.

### Build, Flash, and Run

```
idf.py -p PORT build flash monitor
```

Replace PORT with your serial port.

**Prerequisites:**
- Connect an Ethernet cable to the ESP's Ethernet port (connected to a network with DHCP server)
- Ensure a Wi-Fi access point is available with the configured SSID and password

Both interfaces will attempt to connect and obtain IP addresses via DHCP. The example waits for the Wi-Fi STA interface to get an IP address before completing initialization.

## Example Output

```
I (2706) sta_to_eth_L3: Wi-Fi STA initialized. SSID:xxxxx password:xxxxx
I (2706) sta_to_eth_L3: Wi-Fi Event: base=WIFI_EVENT, id=2
I (2716) sta_to_eth_L3: Wi-Fi STA started
I (2856) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1, snd_ch_cfg:0x0
I (2856) wifi:state: init -> auth (0xb0)
I (2856) wifi:state: auth -> assoc (0x0)
I (2866) wifi:state: assoc -> run (0x10)
I (3006) wifi:connected with xxxxx, aid = 22, channel 1, BW20, bssid = xx:xx:xx:xx:xx:xx
I (3006) wifi:security: WPA2-PSK, phy: bgn, rssi: -58
I (3006) wifi:pm start, type: 1
I (3016) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3026) sta_to_eth_L3: Wi-Fi Event: base=WIFI_EVENT, id=4
I (3026) sta_to_eth_L3: Wi-Fi STA connected
I (3036) wifi:<ba-add>idx:0 (ifx:0, xx:xx:xx:xx:xx:xx), tid:6, ssn:1, winSize:64
I (3146) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (4036) sta_to_eth_L3: Wi-Fi Got IP Address
I (4036) sta_to_eth_L3: Event: base=IP_EVENT, id=0
I (4036) sta_to_eth_L3: ~~~~~~~~~~~
I (4036) sta_to_eth_L3: STAIP:xxx.xxx.xxx.xxx
I (4036) sta_to_eth_L3: STAMASK:255.255.255.0
I (4036) sta_to_eth_L3: STAGW:xxx.xxx.xxx.xxx
I (4046) sta_to_eth_L3: DHCP_DNS_MAIN:xxx.xxx.xxx.xxx
I (4046) sta_to_eth_L3: DHCP_DNS_BACKUP:xxx.xxx.xxx.xxx
I (4056) sta_to_eth_L3: ~~~~~~~~~~~
I (4056) esp_netif_handlers: sta ip: xxx.xxx.xxx.xxx, mask: 255.255.255.0, gw: xxx.xxx.xxx.xxx
I (4066) main_task: Returned from app_main()
I (4966) sta_to_eth_L3: Ethernet Link Up
I (4966) sta_to_eth_L3: Ethernet HW Addr xx:xx:xx:xx:xx:xx
I (4966) sta_to_eth_L3: Ethernet Got IP Address
I (4966) sta_to_eth_L3: Event: base=IP_EVENT, id=4
I (4966) sta_to_eth_L3: ~~~~~~~~~~~
I (4966) sta_to_eth_L3: ETHIP:xxx.xxx.xxx.xxx
I (4966) sta_to_eth_L3: ETHMASK:255.255.255.0
I (4966) sta_to_eth_L3: ETHGW:xxx.xxx.xxx.xxx
I (4976) sta_to_eth_L3: DHCP_DNS_MAIN:xxx.xxx.xxx.xxx
I (4976) sta_to_eth_L3: DHCP_DNS_BACKUP:xxx.xxx.xxx.xxx
I (4986) sta_to_eth_L3: ~~~~~~~~~~~
I (4986) esp_netif_handlers: eth ip: xxx.xxx.xxx.xxx, mask: 255.255.255.0, gw: xxx.xxx.xxx.xxx
```

After both interfaces connect, they will each have their own IP address obtained via DHCP from their respective networks.

## Troubleshooting

- **Ethernet Link Not Up**: Ensure the Ethernet cable is properly connected and the PHY configuration matches your hardware. Verify that the Ethernet network has a DHCP server running.
- **Wi-Fi Not Connecting**: Check that the SSID and password are correct in the source code. Ensure the Wi-Fi access point is within range and operational.
- **No IP Address Received**: Verify that DHCP servers are running on both networks. Check network connectivity and DHCP server availability.
- **Interface Initialization Order**: The example waits for Wi-Fi STA to get an IP before completing initialization. Ethernet will continue to initialize independently.
- **Hardware Issues**: For hardware and driver issues, refer to ESP-IDF Ethernet and Wi-Fi documentation and the upper level [README](../README.md)

## Configuration Notes

- Both interfaces operate independently and can have different IP addresses, gateways, and DNS servers
- The example waits for Wi-Fi STA to obtain an IP address before completing `app_main()`
- Ethernet interface initialization happens in parallel and will obtain its IP address independently
- DNS server information is obtained from DHCP for both interfaces and displayed in the logs
- Wi-Fi credentials are currently hardcoded in the source file (`EXAMPLE_ESP_WIFI_SSID` and `EXAMPLE_ESP_WIFI_PASS`)
