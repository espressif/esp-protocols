/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_netif.h"
#endif // !CONFIG_IDF_TARGET_LINUX

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_NET_CONNECT_WIFI
#define NET_CONNECT_NETIF_DESC_STA "net_connect_netif_sta"
#endif

#if CONFIG_NET_CONNECT_ETHERNET
#define NET_CONNECT_NETIF_DESC_ETH "net_connect_netif_eth"
#endif

#if CONFIG_NET_CONNECT_THREAD
#define NET_CONNECT_NETIF_DESC_THREAD "net_connect_netif_thread"
#endif

#if CONFIG_NET_CONNECT_PPP
#define NET_CONNECT_NETIF_DESC_PPP "net_connect_netif_ppp"
#endif

#if CONFIG_NET_CONNECT_WIFI_SCAN_METHOD_FAST
#define NET_CONNECT_WIFI_SCAN_METHOD WIFI_FAST_SCAN
#elif CONFIG_NET_CONNECT_WIFI_SCAN_METHOD_ALL_CHANNEL
#define NET_CONNECT_WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_NET_CONNECT_WIFI_CONNECT_AP_BY_SIGNAL
#define NET_CONNECT_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_NET_CONNECT_WIFI_CONNECT_AP_BY_SECURITY
#define NET_CONNECT_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_NET_CONNECT_WIFI_AUTH_OPEN
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WEP
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WPA_PSK
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WPA2_PSK
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WPA_WPA2_PSK
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WPA2_ENTERPRISE
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WPA3_PSK
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WPA2_WPA3_PSK
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_NET_CONNECT_WIFI_AUTH_WAPI_PSK
#define NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* Network connect default interface, prefer the ethernet one if running in example-test (CI) configuration */
#if CONFIG_NET_CONNECT_ETHERNET
#define NET_CONNECT_INTERFACE net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_ETH)
#define net_get_netif() net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_ETH)
#elif CONFIG_NET_CONNECT_WIFI
#define NET_CONNECT_INTERFACE net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_STA)
#define net_get_netif() net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_STA)
#elif CONFIG_NET_CONNECT_THREAD
#define NET_CONNECT_INTERFACE net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_THREAD)
#define net_get_netif() net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_THREAD)
#elif CONFIG_NET_CONNECT_PPP
#define NET_CONNECT_INTERFACE net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_PPP)
#define net_get_netif() net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_PPP)
#endif

#if !CONFIG_IDF_TARGET_LINUX
/**
 * @brief Configure Wi-Fi or Ethernet, connect, wait for IP
 *
 * This all-in-one helper function is used in protocols examples to
 * reduce the amount of boilerplate in the example.
 *
 * It is not intended to be used in real world applications.
 * See examples under examples/wifi/getting_started/ and examples/ethernet/
 * for more complete Wi-Fi or Ethernet initialization code.
 *
 * Read "Establishing Wi-Fi or Ethernet Connection" section in
 * examples/protocols/README.md for more information about this function.
 *
 * @return ESP_OK on successful connection
 */
esp_err_t net_connect(void);

/**
 * Counterpart to net_connect, de-initializes Wi-Fi or Ethernet
 */
esp_err_t net_disconnect(void);

/**
 * @brief Configure stdin and stdout to use blocking I/O
 *
 * This helper function is used in ASIO examples. It wraps installing the
 * UART driver and configuring VFS layer to use UART driver for console I/O.
 */
esp_err_t net_configure_stdin_stdout(void);

/**
 * @brief Returns esp-netif pointer created by net_connect() described by
 * the supplied desc field
 *
 * @param desc Textual interface of created network interface, for example "sta"
 * indicate default WiFi station, "eth" default Ethernet interface.
 *
 */
esp_netif_t *net_get_netif_from_desc(const char *desc);

#if CONFIG_NET_CONNECT_WIFI && CONFIG_NET_CONNECT_PROVIDE_WIFI_CONSOLE_CMD
/**
 * @brief Register wifi connect commands
 *
 * Provide a simple wifi_connect command in esp_console.
 * This function can be used after esp_console is initialized.
 */
void net_register_wifi_connect_commands(void);
#endif

#else
static inline esp_err_t net_connect(void)
{
    return ESP_OK;
}
#endif // !CONFIG_IDF_TARGET_LINUX

#if CONFIG_NET_CONNECT_WIFI
#include "net_connect_wifi_config.h"
#endif

#ifdef __cplusplus
}
#endif
