/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Private helpers shared inside the net_connect component. */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "net_iface.h"
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_netif.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_NET_CONNECT_IPV6
/* How many IPv6 addresses to print for one netif. */
#define MAX_IP6_ADDRS_PER_NETIF (5)

#if defined(CONFIG_NET_CONNECT_IPV6_PREF_LOCAL_LINK)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_LINK_LOCAL
#elif defined(CONFIG_NET_CONNECT_IPV6_PREF_GLOBAL)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_GLOBAL
#elif defined(CONFIG_NET_CONNECT_IPV6_PREF_SITE_LOCAL)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_SITE_LOCAL
#elif defined(CONFIG_NET_CONNECT_IPV6_PREF_UNIQUE_LOCAL)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_UNIQUE_LOCAL
#endif

/* Names of the IPv6 address types, in enum order. Defined in connect.c. */
extern const char *net_connect_ipv6_addr_types_to_str[6];
#endif

#if !CONFIG_IDF_TARGET_LINUX

/* True when the netif description starts with prefix. An empty prefix never matches. */
bool net_connect_is_our_netif(const char *prefix, esp_netif_t *netif);

/* Print the addresses of every netif whose description starts with prefix. */
void net_connect_print_all_netif_ips(const char *prefix);

#if CONFIG_NET_CONNECT_WIFI && CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
/*
 * Read one "ssid password" line from stdin and store it on the Wi-Fi handle.
 * This blocks until a line is entered. Call it before connect, not from the
 * Wi-Fi connect callback.
 */
esp_err_t net_connect_wifi_apply_stdin(net_iface_handle_t h);
#endif

#endif /* !CONFIG_IDF_TARGET_LINUX */

#ifdef __cplusplus
}
#endif
