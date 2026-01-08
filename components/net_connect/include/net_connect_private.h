/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*  Private Functions of protocol example common */

#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_NET_CONNECT_IPV6
#define MAX_IP6_ADDRS_PER_NETIF (5)

#if defined(CONFIG_NET_CONNECT_IPV6_PREF_LOCAL_LINK)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_LINK_LOCAL
#elif defined(CONFIG_NET_CONNECT_IPV6_PREF_GLOBAL)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_GLOBAL
#elif defined(CONFIG_NET_CONNECT_IPV6_PREF_SITE_LOCAL)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_SITE_LOCAL
#elif defined(CONFIG_NET_CONNECT_IPV6_PREF_UNIQUE_LOCAL)
#define NET_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_UNIQUE_LOCAL
#endif // if-elif CONFIG_NET_CONNECT_IPV6_PREF_...

#endif


#if CONFIG_NET_CONNECT_IPV6
extern const char *net_connect_ipv6_addr_types_to_str[6];
#endif

esp_err_t net_connect_wifi_start(void);
void net_connect_wifi_stop(void);
esp_err_t net_connect_wifi_sta_do_connect(wifi_config_t wifi_config, bool wait);
esp_err_t net_connect_wifi_sta_do_disconnect(void);
bool net_connect_is_our_netif(const char *prefix, esp_netif_t *netif);
void net_connect_print_all_netif_ips(const char *prefix);
void net_connect_wifi_shutdown(void);
void net_connect_ethernet_shutdown(void);
esp_err_t net_connect_ethernet_connect(void);
void net_connect_thread_shutdown(void);
esp_err_t net_connect_thread_connect(void);
esp_err_t net_connect_ppp_connect(void);
void net_connect_ppp_start(void);
void net_connect_ppp_shutdown(void);



#ifdef __cplusplus
}
#endif
