| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# ESP DNS Component

This component provides a flexible DNS resolution system for ESP32 devices with support for multiple DNS protocols. It allows applications to securely resolve domain names using various transport methods, including standard UDP/TCP DNS, DNS over TLS (DoT), and DNS over HTTPS (DoH).

## Table of Contents
- [Features](#features)
- [How It Works](#how-it-works)
- [Certificate Options](#certificate-options)
- [How to Use](#how-to-use)
- [Configuration](#configuration)
- [Common DNS Servers](#common-dns-servers)
- [Requirements](#requirements)
- [Limitations](#limitations)
- [Performance Considerations](#performance-considerations)
- [Troubleshooting](#troubleshooting)

## Features

- **Multiple Protocol Support** Choose from various DNS protocols:
  - Standard UDP DNS (Port 53)
  - TCP DNS (Port 53)
  - DNS over TLS (DoT) (Port 853)
  - DNS over HTTPS (DoH) (Port 443)

- **Secure DNS Resolution**: Supports encrypted DNS queries using TLS and HTTPS to protect privacy and prevent DNS spoofing.

- **Flexible Configuration**: Easily configure DNS servers, ports, timeouts, and protocol-specific options.

- **LWIP Integration**: Seamlessly integrates with the ESP-IDF networking stack through LWIP hooks.

- **Standard getaddrinfo() Interface**: Use the standard `getaddrinfo()` function to resolve domain names.

## How It Works
This component utilizes the `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM` hook to override the core DNS functionality of LWIP and implement custom DNS over HTTPS resolution. To enable this, ensure that the configuration option `Component config → LWIP → Hooks → Netconn external resolve Hook` is set to `Custom implementation`.

Once you add this component to your project, it will replace the default LWIP DNS resolution automatically.

**⚠️ Warning:** This component cannot work alongside other components that use the CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM hook, such as the OpenThread component.

## Certificate Options

When using secure DNS protocols (DoT and DoH), you have two certificate options:

1. **Certificate Bundle**: Use ESP-IDF's certificate bundle for validating connections to popular DNS providers.
2. **Custom Certificate**: Provide your own certificate in PEM format for custom DNS servers.


## How to Use

### 1. Add Component to Your Project

Add the ESP DNS component to your project's `components` directory or include it as a dependency in your project's `idf_component.yml` file.

### 2. Enable custom DNS resolution

To enable custom DNS resolution, configure the `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM` setting either through menuconfig or by adding `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM=y` to your `sdkconfig.defaults` file to pre-set the configuration during the build process.

### 3. Configure DNS Settings

Initialize the DNS component with your preferred configuration:
```C
#include "esp_dns.h"

// Configure DNS over HTTPS
esp_dns_config_t dns_config = {
    .protocol = DNS_PROTOCOL_DOH,
    .dns_server = "dns.google",
    .port = 443, // Optional, default is 443
    .timeout_ms = 5000, // Optional, default is 5000
    .tls_config = {
        .crt_bundle_attach = esp_crt_bundle_attach, // Optional, default is NULL
    },
    .protocol_config.doh_config = {
        .url_path = "/dns-query", // Optional, default is "/dns-query"
    }
};

// Initialize DNS component
esp_dns_handle_t dns_handle = esp_dns_init(&dns_config);
if (dns_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize DNS");
    return;
}
```


### 4. Resolve Domain Names

Once initialized, the component automatically handles DNS resolution through the standard `getaddrinfo()` function:

```C
struct addrinfo hints = {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
};
struct addrinfo res;
int err = getaddrinfo("www.example.com", "80", &hints, &res);
if (err != 0) {
    ESP_LOGE(TAG, "DNS lookup failed: %d", err);
    return;
}
// Use the resolved addresses
// ...
// Free the address info when done
freeaddrinfo(res);
```

### 5. Cleanup

When you're done using the DNS component, clean up resources:

```C
esp_dns_cleanup(dns_handle);
```

## Configuration

###Setting Up the ESP DNS Component

1. Navigate to your project directory.
2. Execute `idf.py menuconfig`.
3. Locate the **Component config -> LWIP -> Hooks -> Netconn external resolve Hook** section.
4. Change the setting to `Custom implementation`.

### Common Settings

| Parameter | Description | Default Value |
|-----------|-------------|---------------|
| `protocol` | DNS protocol (UDP, TCP, DoT, DoH) | `DNS_PROTOCOL_UDP` |
| `dns_server` | IP address or hostname of DNS server | `"8.8.8.8"` (Google DNS) |
| `port` | Server port number | Protocol-dependent (53, 853, or 443) |
| `timeout_ms` | Query timeout in milliseconds | `5000` (5 seconds) |

### TLS Configuration (for DoT and DoH)

| Parameter | Description |
|-----------|-------------|
| `crt_bundle_attach` | Function pointer to attach certificate bundle |
| `server_cert` | SSL server certificate in PEM format |
| `alpn_protos` | ALPN protocols for DoH (typically `"h2"`) |

### Protocol-Specific Options

#### DoH Options
- **URL Path**: URL path for DoH service (e.g., "/dns-query")

## Requirements

- ESP-IDF v5.0 or newer
- Network connectivity (Wi-Fi or Ethernet)
- For DoT/DoH: Sufficient RAM for TLS operations

## Limitations

- The UDP DNS protocol implementation relies on the native LWIP DNS resolver.
- Transport protocol selection must be configured through `esp_dns_init()` rather than `getaddrinfo()` parameters due to LWIP resolver hook limitations.
- Maximum response size is limited by the buffer size (default: 512 bytes) for DNS over TLS (DOT) and TCP protocols.
- Only one DNS protocol can be active at a time.

- **Resolution Speed**:
  - UDP DNS is fastest but least secure
  - DoH typically has the highest latency but offers the best security

## Performance Considerations

- **Memory Usage**: DoH and DoT require more memory due to TLS overhead:
TBD: Fill in the memory usage for each protocol


## Troubleshooting

- **Connection Issues**:
  - Ensure network connectivity and correct DNS server configuration
  - Verify that your network allows the required ports (53, 853, or 443)

- **Certificate Errors**:
  - Verify that the correct certificate is provided for secure protocols
  - For public DNS servers, use the certificate bundle approach

- **Timeout Errors**:
  - Increase the timeout value for slow network connections
  - Try a different DNS server that might be geographically closer

- **Memory Issues**:
  - If you encounter memory errors, consider increasing the task stack size
  - For memory-constrained devices, prefer UDP DNS.
