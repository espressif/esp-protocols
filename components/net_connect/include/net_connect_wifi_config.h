/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_NET_CONNECT_WIFI

/* Forward declaration for interface handle */
typedef void* net_iface_handle_t;

/* ============================================================
 *                Default Configuration Macros
 * ============================================================ */

/* General network configuration defaults */
#define NET_CONNECT_DEFAULT_USE_DHCP        true
#define NET_CONNECT_DEFAULT_ENABLE_IPV6     false

/* WiFi-specific defaults */
#define NET_CONNECT_WIFI_DEFAULT_MAX_RETRY      5
#define NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO     128

/**
 * @brief  IP configuration (reusable for any interface, supports IPv4 and IPv6)
 */
typedef struct {
    /* ===== IPv4 Configuration ===== */
    bool use_dhcp;                // true = use DHCP, false = use static IPv4
    char ip[16];                  // IPv4 address (if use_dhcp = false)
    char netmask[16];             // Subnet mask
    char gateway[16];             // Gateway address
    char dns_main[16];            // Primary DNS server (IPv4)
    char dns_backup[16];          // Secondary DNS server (IPv4)
    char dns_fallback[16];        // Fallback DNS server (IPv4)

    /* ===== IPv6 Configuration ===== */
    bool enable_ipv6;             // true = enable IPv6 on this interface
    int ipv6_mode;                // 0 = SLAAC, 1 = Manual, 2 = DHCPv6
    char ipv6_addr[40];           // Static IPv6 address (if ipv6_mode = Manual)
    char ipv6_gateway[40];        // IPv6 gateway address
    char ipv6_dns_main[40];       // Primary DNS server (IPv6)
    char ipv6_dns_backup[40];     // Secondary DNS server (IPv6)
} net_ip_config_t;

/**
 * @brief Wi-Fi Station (STA) configuration
 */
typedef struct {
    char ssid[32];
    char password[64];
    int scan_method;            // Active or Passive scan
    int sort_method;            // Sort by signal or by security
    int threshold_rssi;         // Minimum RSSI threshold for connection
    int auth_mode_threshold;    // Weakest acceptable authentication type
    int max_retry;              // Maximum retry attempts before giving up
    net_ip_config_t ip;         // IP settings for STA (DHCP or static)
} net_wifi_sta_config_t;

/**
 * @brief Configure Wi-Fi Station (STA) interface.
 *
 * Stores STA configuration and returns a handle for later connection.
 * Passing NULL uses default/Kconfig settings.
 *
 * @param[in] config Pointer to Wi-Fi STA configuration (can be NULL)
 * @return Interface handle for STA interface, or NULL on error
 */
net_iface_handle_t net_configure_wifi_sta(net_wifi_sta_config_t *config);

/**
 * @brief Connect Wi-Fi interface using stored configuration.
 *
 * Connects the Wi-Fi STA interface using the configuration previously
 * stored via net_configure_wifi_sta().
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if WiFi is not configured
 *  - ESP_FAIL or error code on failure
 */
esp_err_t net_connect_wifi(void);

/**
 * @brief Disconnect Wi-Fi interface.
 *
 * Disconnects the Wi-Fi STA interface that was connected via net_connect_wifi().
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL or error code on failure
 */
esp_err_t net_disconnect_wifi(void);

/**
 * @brief Check if WiFi is configured via new API.
 *
 * Internal function to check if WiFi was configured using the new API.
 *
 * @return true if configured, false otherwise
 */
bool net_connect_wifi_is_configured(void);

#endif /* CONFIG_NET_CONNECT_WIFI */

#ifdef __cplusplus
}
#endif
