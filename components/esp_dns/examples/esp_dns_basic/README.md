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
I (4583) example_esp_dns: Executing DNS without initializing ESP_DNS module
I (4603) wifi:<ba-add>idx:1 (ifx:0, a0:36:bc:0e:c4:f0), tid:7, ssn:3, winSize:64
I (4613) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (4613) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (4613) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (4613) wifi:<ba-del>idx:0, tid:6
I (4623) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (4623) wifi:<ba-add>idx:0 (ifx:0, a0:36:bc:0e:c4:f0), tid:0, ssn:1, winSize:64
I (4643) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (4643) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (4643) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (4653) example_esp_dns: Free Heap: 215292 bytes, Min Free Heap: 206008 bytes, Stack High Water Mark: 1220 bytes

I (4663) example_esp_dns: Executing UDP DNS
I (4673) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (4673) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (4683) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (4683) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (4693) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (4703) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (4703) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (4713) example_esp_dns: Free Heap: 215116 bytes, Min Free Heap: 206008 bytes, Stack High Water Mark: 1220 bytes

I (4723) example_esp_dns: Executing TCP DNS
I (4763) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (4763) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (4763) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (4763) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (4793) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (4793) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (4793) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (4803) example_esp_dns: Free Heap: 214580 bytes, Min Free Heap: 206008 bytes, Stack High Water Mark: 1220 bytes

I (4813) example_esp_dns: Executing DNS over TLS
I (5963) example_esp_dns: Hostname: yahoo.com: 74.6.143.25(IPv4)
I (5963) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (5963) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (5973) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (7083) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (7083) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (7083) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (7093) example_esp_dns: Free Heap: 213504 bytes, Min Free Heap: 165308 bytes, Stack High Water Mark: 1220 bytes

I (7103) example_esp_dns: Executing DNS over TLS
I (7413) esp-x509-crt-bundle: Certificate validated
I (8233) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (8233) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (8233) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (8243) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (8553) esp-x509-crt-bundle: Certificate validated
I (9363) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (9363) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (9363) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (9373) example_esp_dns: Free Heap: 213120 bytes, Min Free Heap: 165308 bytes, Stack High Water Mark: 1220 bytes

I (9383) example_esp_dns: Executing DNS over HTTPS
I (10563) example_esp_dns: Hostname: yahoo.com: 74.6.143.26(IPv4)
I (10563) example_esp_dns: Hostname: yahoo.com: 74.6.231.20(IPv4)
I (10563) example_esp_dns: Hostname: yahoo.com: 74.6.143.25(IPv4)
I (10573) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (11713) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (11713) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (11723) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (11723) example_esp_dns: Free Heap: 212664 bytes, Min Free Heap: 162780 bytes, Stack High Water Mark: 1220 bytes

I (11733) example_esp_dns: Executing DNS over HTTPS
I (12033) esp-x509-crt-bundle: Certificate validated
I (12863) example_esp_dns: Hostname: yahoo.com: 74.6.231.21(IPv4)
I (12863) example_esp_dns: Hostname: yahoo.com: 98.137.11.163(IPv4)
I (12863) example_esp_dns: Hostname: yahoo.com: 98.137.11.164(IPv4)
I (12873) example_esp_dns: Hostname: yahoo.com: 74.6.143.25(IPv4)
I (13153) esp-x509-crt-bundle: Certificate validated
I (13993) example_esp_dns: Hostname: www.google.com: 2404:6800:4015:803::2004(IPv6)
I (13993) example_esp_dns: Hostname: 0.0.0.0: 0.0.0.0(IPv4)
I (13993) example_esp_dns: Hostname: fe80:0000:0000:0000:5abf:25ff:fee0:4100: FE80::5ABF:25FF:FEE0:4100(IPv6)
I (14003) example_esp_dns: Free Heap: 212044 bytes, Min Free Heap: 162780 bytes, Stack High Water Mark: 1220 bytes

I (14013) main_task: Returned from app_main()
```
