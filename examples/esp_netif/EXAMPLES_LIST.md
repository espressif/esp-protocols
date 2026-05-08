# Network Interface Configuration Examples

This document lists network interface configuration examples in this tree. The examples **2ethDHCP**, **2ethStaticIp**, **EthStaEthAp**, **WifiApWithServer**, and **wifiSta** are maintained in the companion **NAPT** repository (same directory names at the NAPT project root).

### 1. **eth_gateway**
   - **Description**: Single Ethernet interface as **gateway** (DHCP server on the cable; downstream subnet—not a Wi-Fi AP)
   - **Location**: `eth_gateway/`
   - **Features**: One Ethernet interface, DHCP server, static gateway IP

### 2. **eth_gateway_wifi_ap**
   - **Description**: Ethernet **gateway** + Wi-Fi access point (DHCP on both)
   - **Location**: `eth_gateway_wifi_ap/`
   - **Features**: Wired gateway role plus Wi-Fi AP for wireless clients

### 3. **eth_gateway_wifi_sta**
   - **Description**: Ethernet **gateway** + Wi-Fi station (upstream Wi-Fi, DHCP server on Ethernet)
   - **Location**: `eth_gateway_wifi_sta/`
   - **Features**: Ethernet gateway with Wi-Fi STA backhaul

### 4. **eth_endpoint_wifi_ap**
   - **Description**: Ethernet **endpoint** (DHCP client) + Wi-Fi access point
   - **Location**: `eth_endpoint_wifi_ap/`
   - **Features**: Wired DHCP client toward upstream; local Wi-Fi AP

### 5. **eth_endpoint_wifi_sta**
   - **Description**: Ethernet **endpoint** (DHCP client) + Wi-Fi station (dual DHCP clients)
   - **Location**: `eth_endpoint_wifi_sta/`
   - **Features**: Both interfaces as DHCP clients

Additional examples (e.g. **WifiApWifiSta**) may live in the **NAPT** repository alongside the examples named in the introduction.
