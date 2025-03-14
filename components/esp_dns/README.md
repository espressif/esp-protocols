# ESP DNS Component

This component provides a flexible DNS resolution system for ESP32 devices with support for multiple DNS protocols. It allows applications to resolve domain names using various transport methods, including standard UDP/TCP DNS, and securely resolve them using DNS over TLS (DoT) and DNS over HTTPS (DoH).

## Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [How to Use](#how-to-use)
- [Configuration](#configuration)
- [Certificate Options](#certificate-options)
- [Limitations](#limitations)
- [Performance Considerations](#performance-considerations)
- [How It Works](#how-it-works)
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


## Requirements

- ESP-IDF v5.0 or newer
- Network connectivity (Wi-Fi or Ethernet)
- For DoT/DoH: Sufficient RAM for TLS operations


## How to Use

### 1. Enable custom DNS resolution

To enable custom DNS resolution, configure the `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM` setting either through menuconfig or by adding `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM=y` to your `sdkconfig.defaults` file to pre-set the configuration during the build process.

### 2. Configure DNS Settings

Initialize the DNS component with your preferred configuration:
```C
#include "esp_dns.h"

/* Configure DNS over HTTPS */
esp_dns_config_t dns_config = {
    .dns_server = "dns.google",            /* DNS server hostname or IP address */
    .port = ESP_DNS_DEFAULT_DOH_PORT,      /* Optional: Server port (443 is default for HTTPS) */
    .timeout_ms = ESP_DNS_DEFAULT_TIMEOUT_MS, /* Optional: Request timeout in milliseconds (10000ms default) */
    .tls_config = {
        /* Optional: Use ESP-IDF certificate bundle for validating popular DNS providers */
        .crt_bundle_attach = esp_crt_bundle_attach,

        /* Or provide a custom certificate in PEM format (string) for your DNS server */
        /* Note: Only PEM format is supported; DER format certificates are not supported yet */
        .cert_pem = server_root_cert_pem_start,

        /* Note: If both crt_bundle_attach and cert_pem are provided,
           crt_bundle_attach is preferred over cert_pem */
    },
    .protocol_config.doh_config = {
        .url_path = "/dns-query",          /* Optional: DoH endpoint path on the server ("/dns-query" default) */
    }
};

/* Initialize DNS component based on protocol */
esp_dns_handle_t dns_handle = NULL;

/* Call esp_dns_init_doh() to use DNS over HTTPS */
dns_handle = esp_dns_init_doh(&dns_config);

/* or Call esp_dns_init_dot() to use DNS over TLS */
dns_handle = esp_dns_init_dot(&dns_config);

/* or Call esp_dns_init_tcp() to use DNS over TCP */
dns_handle = esp_dns_init_tcp(&dns_config);

/* or Call esp_dns_init_udp() to use DNS over UDP */
dns_handle = esp_dns_init_udp(&dns_config);

if (dns_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize DNS");
    return;
}
```


### 3. Resolve Domain Names

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
/* Use the resolved addresses */
/* ... */

/* Free the address info when done */
freeaddrinfo(res);
```

### 4. Cleanup

When you're done using the DNS component, clean up resources based on the protocol used:

```C
int ret = 0;
/* Call esp_dns_cleanup_doh() to cleanup DNS over HTTPS */
ret = esp_dns_cleanup_doh(dns_handle);

/* or Call esp_dns_cleanup_dot() to cleanup DNS over TLS */
ret = esp_dns_cleanup_dot(dns_handle);

/* or Call esp_dns_cleanup_tcp() to cleanup DNS over TCP */
ret = esp_dns_cleanup_tcp(dns_handle);

/* or Call esp_dns_cleanup_udp() to cleanup DNS over UDP */
ret = esp_dns_cleanup_udp(dns_handle);

if (ret != 0) {
    ESP_LOGE(TAG, "Failed to cleanup DNS");
}
```


## Configuration

### Setting Up the ESP DNS Component

1. Navigate to your project directory.
2. Execute `idf.py menuconfig`.
3. Locate the **Component config -> LWIP -> Hooks -> Netconn external resolve Hook** section.
4. Change the setting to `Custom implementation`.

### Common Settings

| Parameter | Description | Default Value |
|-----------|-------------|---------------|
| `dns_server` | IP address or hostname of DNS server | `"8.8.8.8"` (Google DNS) |
| `port` | Server port number | Protocol-dependent (53, 853, or 443) |
| `timeout_ms` | Query timeout in milliseconds | `10000` (10 seconds) |

### TLS Configuration (for DoT and DoH)

| Parameter | Description |
|-----------|-------------|
| `crt_bundle_attach` | Function pointer to attach certificate bundle |
| `server_cert` | SSL server certificate in PEM format |
| `alpn_protos` | ALPN protocols for DoH (typically `"h2"`) |

### Protocol-Specific Options

#### DoH Options
- **URL Path**: URL path for DoH service (e.g., "/dns-query")


## Certificate Options

When using secure DNS protocols (DoT and DoH), you have two certificate options:

1. **Certificate Bundle**: Use ESP-IDF's certificate bundle for validating connections to popular DNS providers.
2. **Custom Certificate**: Provide your own certificate in PEM format for custom DNS servers.


## Limitations

- The UDP DNS protocol implementation relies on the native LWIP DNS resolver.
- Transport protocol selection must be configured through `esp_dns_init_xxx()` rather than `getaddrinfo()` parameters due to LWIP resolver hook limitations.
- Maximum response size is limited by the buffer size (default: 512 bytes) for DNS over TLS (DOT) and TCP protocols.
- Only one DNS protocol can be active at a time.

- **Resolution Speed**:
  - UDP DNS is fastest but least secure
  - DoH typically has the highest latency but offers the best security


## Performance Considerations

- **Memory Usage**: DoH and DoT require more memory due to TLS overhead:

TBD: Fill in the memory usage for each protocol


## How It Works
This component utilizes the `CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM` hook to override the core DNS functionality of LWIP and implement custom DNS over HTTPS resolution. To enable this, ensure that the configuration option `Component config → LWIP → Hooks → Netconn external resolve Hook` is set to `Custom implementation`.

Once you add this component to your project, it will replace the default LWIP DNS resolution automatically.

**⚠️ Warning:** This component cannot work alongside other components that use the CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM hook, such as the OpenThread component.


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
