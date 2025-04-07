| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# ESP DNS Example

This example demonstrates how to use the ESP DNS component in an ESP32 application. The example resolves domain names using various DNS protocols including standard UDP, TCP, DNS over TLS (DoT), and DNS over HTTPS (DoH).

## Features

- **Standard UDP DNS**: Traditional DNS resolution over UDP
- **DNS over TCP**: DNS resolution using TCP transport
- **DNS over TLS (DoT)**: Secure DNS resolution using TLS encryption
- **DNS over HTTPS (DoH)**: Secure DNS resolution using HTTPS

## Certificate Options

This example provides two certificate options for secure DNS protocols (DoT and DoH):

1. **Certificate Bundle (Default)**: Uses the ESP-IDF certificate bundle, making it easy to get started with popular DNS providers like Google.
2. **Custom Certificate**: Uses a specific certificate for the DNS server. The example includes a Google DNS certificate.

## How It Works

1. **Network Initialization**: The application initializes the network interfaces (Wi-Fi or Ethernet) and establishes a connection.
2. **DNS Resolution Tests**: The example performs DNS resolution using different protocols:
   - Native UDP DNS (system default)
   - ESP DNS with UDP protocol
   - ESP DNS with TCP protocol
   - ESP DNS with DoT protocol (using server certificate)
   - ESP DNS with DoT protocol (using certificate bundle)
   - ESP DNS with DoH protocol (using server certificate)
   - ESP DNS with DoH protocol (using certificate bundle)
3. **Domain Resolution**: For each protocol, the application resolves several domain names including:
   - yahoo.com
   - www.google.com
   - IP addresses (0.0.0.0 and IPv6 address)

## How to use example

Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.

### Hardware Required

* A development board with ESP32/ESP32-S2/ESP32-C3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

idf.py -p PORT flash monitor


(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Troubleshooting Tips

* **Connectivity**:
  Ensure that the network connection details are accurate. For example, verify the Wi-Fi SSID and password or check that the Ethernet connection is secure and not faulty.

* **Memory Issues**:
  If you encounter memory-related errors, check the system information output which displays free heap and stack high water mark. You may need to increase task stack sizes for more complex DNS operations.

* **Certificate Issues**:
  For DoT and DoH protocols, ensure that the certificates are valid for the DNS server you're using. The example includes Google DNS certificates, but these may need to be updated if they expire.

## Example Output

```
I (3188) wifi:new:<12,0>, old:<1,0>, ap:<255,255>, sta:<12,0>, prof:1, snd_ch_cfg:0x0
I (3188) wifi:state: init -> auth (0xb0)
I (3218) wifi:state: auth -> assoc (0x0)
I (3238) wifi:state: assoc -> run (0x10)
I (3368) wifi:connected with Avrow, aid = 19, channel 12, BW20, bssid = a0:36:bc:0e:c4:f0
I (3368) wifi:security: WPA2-PSK, phy: bgn, rssi: -52
I (3368) wifi:pm start, type: 1

I (3368) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3408) wifi:<ba-add>idx:0 (ifx:0, a0:36:bc:0e:c4:f0), tid:6, ssn:2, winSize:64
I (3418) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (4398) esp_netif_handlers: example_netif_sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (4398) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.50.47
I (4558) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:26d7:ebff:febb:f218, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (4558) example_common: Connected to example_netif_sta
I (4568) example_common: - IPv4 address: 192.168.50.47,
I (4568) example_common: - IPv6 address: fe80:0000:0000:0000:26d7:ebff:febb:f218, type: ESP_IP6_ADDR_IS_LINK_LOCAL

I (4578) example_esp_dns: Executing DNS without initializing ESP_DNS module
I (4598) wifi:<ba-add>idx:1 (ifx:0, a0:36:bc:0e:c4:f0), tid:7, ssn:3, winSize:64
I (4598) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (4598) example_esp_dns: Hostname: yahoo.com: 74.6.143.25(IPv4)
I (4598) wifi:<ba-del>idx:0, tid:6
I (4608) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (4608) wifi:<ba-add>idx:0 (ifx:0, a0:36:bc:0e:c4:f0), tid:0, ssn:1, winSize:64
I (4618) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (4628) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (4638) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (4638) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (4648) example_esp_dns: Free Heap: 220608 bytes, Min Free Heap: 210320 bytes, Stack High Water Mark: 1048 bytes

I (4658) example_esp_dns: Executing UDP DNS
I (4658) ESP_DNS_UDP: Initializing UDP DNS
I (4668) ESP_DNS: DNS module initialized successfully with protocol 0
I (4668) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (4678) example_esp_dns: Hostname: yahoo.com: 74.6.143.25(IPv4)
I (4688) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (4688) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (4698) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (4698) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (4708) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (4718) example_esp_dns: Free Heap: 220296 bytes, Min Free Heap: 210320 bytes, Stack High Water Mark: 1048 bytes
I (4728) ESP_DNS_UDP: Cleaning up UDP DNS
I (4728) ESP_DNS: DNS module cleaned up successfully

I (4738) example_esp_dns: Executing DNS over TCP with server cert
I (4738) ESP_DNS_TCP: Initializing TCP DNS
I (4748) ESP_DNS: DNS module initialized successfully with protocol 1
I (4778) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (4778) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (4778) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (4788) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (4818) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (4818) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (4818) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (4828) example_esp_dns: Free Heap: 219848 bytes, Min Free Heap: 210320 bytes, Stack High Water Mark: 1048 bytes
I (4838) ESP_DNS_TCP: Cleaning up TCP DNS
I (4838) ESP_DNS: DNS module cleaned up successfully

I (4848) example_esp_dns: Executing DNS over TLS with server cert
I (4848) ESP_DNS_DOT: Initializing DNS over TLS
I (4858) ESP_DNS: DNS module initialized successfully with protocol 2
I (5878) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (5878) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (5878) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (5888) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (6978) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (6978) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (6978) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (6988) example_esp_dns: Free Heap: 218948 bytes, Min Free Heap: 173612 bytes, Stack High Water Mark: 1048 bytes
I (6998) ESP_DNS_DOT: Cleaning up DNS over TLS
I (6998) ESP_DNS: DNS module cleaned up successfully

I (7008) example_esp_dns: Executing DNS over TLS with cert bundle
I (7008) ESP_DNS_DOT: Initializing DNS over TLS
I (7018) ESP_DNS: DNS module initialized successfully with protocol 2
I (7358) esp-x509-crt-bundle: Certificate validated
I (8158) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (8158) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (8158) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (8158) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (8478) esp-x509-crt-bundle: Certificate validated
I (9278) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (9278) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (9278) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (9288) example_esp_dns: Free Heap: 218504 bytes, Min Free Heap: 173612 bytes, Stack High Water Mark: 1048 bytes
I (9298) ESP_DNS_DOT: Cleaning up DNS over TLS
I (9308) ESP_DNS: DNS module cleaned up successfully

I (9308) example_esp_dns: Executing DNS over HTTPS with server cert
I (9318) ESP_DNS_DOH: Initializing DNS over HTTPS
I (9318) ESP_DNS: DNS module initialized successfully with protocol 3
I (10368) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (10368) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (10368) example_esp_dns: Hostname: yahoo.com: 74.6.143.25(IPv4)
I (10378) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (11508) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (11508) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (11508) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (11518) example_esp_dns: Free Heap: 218068 bytes, Min Free Heap: 170516 bytes, Stack High Water Mark: 1048 bytes
I (11528) ESP_DNS_DOH: Cleaning up DNS over HTTPS
I (11528) ESP_DNS: DNS module cleaned up successfully

I (11538) example_esp_dns: Executing DNS over HTTPS with cert bundle
I (11548) ESP_DNS_DOH: Initializing DNS over HTTPS
I (11548) ESP_DNS: DNS module initialized successfully with protocol 3
I (11858) esp-x509-crt-bundle: Certificate validated
I (12668) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (12678) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (12678) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (12678) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (12948) esp-x509-crt-bundle: Certificate validated
I (13748) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (13748) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (13748) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (13758) example_esp_dns: Free Heap: 217620 bytes, Min Free Heap: 170516 bytes, Stack High Water Mark: 1048 bytes
I (13768) ESP_DNS_DOH: Cleaning up DNS over HTTPS
I (13778) ESP_DNS: DNS module cleaned up successfully
I (13778) main_task: Returned from app_main()
```
