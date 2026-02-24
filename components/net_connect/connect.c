/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "net_connect.h"
#include "net_connect_private.h"
#include "sdkconfig.h"
#if CONFIG_NET_CONNECT_WIFI
#include "net_connect_wifi_config.h"
#endif
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "net_connect";

#if CONFIG_NET_CONNECT_IPV6
/* types of ipv6 addresses to be displayed on ipv6 events */
const char *net_connect_ipv6_addr_types_to_str[6] = {
    "ESP_IP6_ADDR_IS_UNKNOWN",
    "ESP_IP6_ADDR_IS_GLOBAL",
    "ESP_IP6_ADDR_IS_LINK_LOCAL",
    "ESP_IP6_ADDR_IS_SITE_LOCAL",
    "ESP_IP6_ADDR_IS_UNIQUE_LOCAL",
    "ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6"
};
#endif

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
bool net_connect_is_our_netif(const char *prefix, esp_netif_t *netif)
{
    if (prefix == NULL || *prefix == '\0') {
        return false;
    }
    const char *netif_desc = esp_netif_get_desc(netif);
    if (netif_desc == NULL) {
        return false;
    }
    return strncmp(prefix, netif_desc, strlen(prefix)) == 0;
}

/**
 * @brief Predicate function to match network interface by description string
 *
 * Note: The function signature must match esp_netif_find_predicate_t typedef
 * which requires 'void *ctx' as the second parameter, not 'char *ctx'.
 * This is because esp_netif_find_if() uses a generic callback interface that
 * accepts any context type as void*. We cast it to const char* internally.
 */
static bool netif_desc_matches_with(esp_netif_t *netif, void *ctx)
{
    const char *desc = (const char *)ctx;
    const char *netif_desc = esp_netif_get_desc(netif);
    if (netif_desc == NULL) {
        return false;
    }
    return strcmp(desc, netif_desc) == 0;
}

esp_netif_t *net_get_netif_from_desc(const char *desc)
{
    if (desc == NULL || *desc == '\0') {
        return NULL;
    }
    return esp_netif_find_if(netif_desc_matches_with, (void*)desc);
}

static esp_err_t print_all_ips_tcpip(void* ctx)
{
    const char *prefix = ctx;
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t *netif = NULL;
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        if (net_connect_is_our_netif(prefix, netif)) {
            const char *netif_desc = esp_netif_get_desc(netif);
            ESP_LOGI(TAG, "Connected to %s", netif_desc != NULL ? netif_desc : "(null)");
#if CONFIG_NET_CONNECT_IPV4
            esp_netif_ip_info_t ip;
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&ip.ip));
#endif
#if CONFIG_NET_CONNECT_IPV6
            esp_ip6_addr_t ip6[MAX_IP6_ADDRS_PER_NETIF];
            int ip6_addrs = esp_netif_get_all_ip6(netif, ip6);
            for (int j = 0; j < ip6_addrs; ++j) {
                esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&(ip6[j]));
                ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %s", IPV62STR(ip6[j]), net_connect_ipv6_addr_types_to_str[ipv6_type]);
            }
#endif
        }
    }
    return ESP_OK;
}

void net_connect_print_all_netif_ips(const char *prefix)
{
    // Print all IPs in TCPIP context to avoid potential races of removing/adding netifs when iterating over the list
    esp_netif_tcpip_exec(print_all_ips_tcpip, (void*) prefix);
}


esp_err_t net_connect(void)
{
#if CONFIG_NET_CONNECT_ETHERNET
    bool eth_initialized = false;
#endif
#if CONFIG_NET_CONNECT_WIFI
    bool wifi_initialized = false;
#endif
#if CONFIG_NET_CONNECT_THREAD
    bool thread_initialized = false;
#endif
#if CONFIG_NET_CONNECT_PPP
    bool ppp_initialized = false;
#endif

#if CONFIG_NET_CONNECT_ETHERNET
    ESP_LOGI(TAG, "Initializing Ethernet interface...");
    if (net_connect_ethernet_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Ethernet interface");
        goto cleanup;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&net_connect_ethernet_shutdown));
    eth_initialized = true;
    ESP_LOGI(TAG, "Ethernet interface initialized successfully");
#endif
#if CONFIG_NET_CONNECT_WIFI
    ESP_LOGI(TAG, "Initializing WiFi interface...");
    if (!net_connect_wifi_is_configured()) {
        /* Configure WiFi with Kconfig defaults */
        if (net_configure_wifi_sta(NULL) == NULL) {
            ESP_LOGE(TAG, "Failed to configure WiFi interface");
            goto cleanup;
        }
    }
    /* Use new API for WiFi connection */
    if (net_connect_wifi() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi interface");
        goto cleanup;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&net_connect_wifi_shutdown));
    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi interface initialized successfully");
#endif
#if CONFIG_NET_CONNECT_THREAD
    ESP_LOGI(TAG, "Initializing Thread interface...");
    if (net_connect_thread_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Thread interface");
        goto cleanup;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&net_connect_thread_shutdown));
    thread_initialized = true;
    ESP_LOGI(TAG, "Thread interface initialized successfully");
#endif
#if CONFIG_NET_CONNECT_PPP
    ESP_LOGI(TAG, "Initializing PPP interface...");
    if (net_connect_ppp_connect() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PPP interface");
        goto cleanup;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&net_connect_ppp_shutdown));
    ppp_initialized = true;
    ESP_LOGI(TAG, "PPP interface initialized successfully");
#endif

#if CONFIG_NET_CONNECT_ETHERNET
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_ETH);
#endif

#if CONFIG_NET_CONNECT_WIFI
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_STA);
#endif

#if CONFIG_NET_CONNECT_THREAD
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_THREAD);
#endif

#if CONFIG_NET_CONNECT_PPP
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_PPP);
#endif

    return ESP_OK;

cleanup:
    /* Clean up previously initialized interfaces in reverse order */
#if CONFIG_NET_CONNECT_PPP
    if (ppp_initialized) {
        net_connect_ppp_shutdown();
        esp_unregister_shutdown_handler(&net_connect_ppp_shutdown);
    }
#endif
#if CONFIG_NET_CONNECT_THREAD
    if (thread_initialized) {
        net_connect_thread_shutdown();
        esp_unregister_shutdown_handler(&net_connect_thread_shutdown);
    }
#endif
#if CONFIG_NET_CONNECT_WIFI
    if (wifi_initialized) {
        net_connect_wifi_shutdown();
        esp_unregister_shutdown_handler(&net_connect_wifi_shutdown);
    }
#endif
#if CONFIG_NET_CONNECT_ETHERNET
    if (eth_initialized) {
        net_connect_ethernet_shutdown();
        esp_unregister_shutdown_handler(&net_connect_ethernet_shutdown);
    }
#endif
    return ESP_FAIL;
}


esp_err_t net_disconnect(void)
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Disconnecting network interfaces...");
#if CONFIG_NET_CONNECT_ETHERNET
    ESP_LOGI(TAG, "Deinitializing Ethernet interface...");
    net_connect_ethernet_shutdown();
    ret = esp_unregister_shutdown_handler(&net_connect_ethernet_shutdown);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG, "Ethernet interface deinitialized");
#endif
#if CONFIG_NET_CONNECT_WIFI
    ESP_LOGI(TAG, "Deinitializing WiFi interface...");
    net_connect_wifi_shutdown();
    ret = esp_unregister_shutdown_handler(&net_connect_wifi_shutdown);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG, "WiFi interface deinitialized");
#endif
#if CONFIG_NET_CONNECT_THREAD
    ESP_LOGI(TAG, "Deinitializing Thread interface...");
    net_connect_thread_shutdown();
    ret = esp_unregister_shutdown_handler(&net_connect_thread_shutdown);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG, "Thread interface deinitialized");
#endif
#if CONFIG_NET_CONNECT_PPP
    ESP_LOGI(TAG, "Deinitializing PPP interface...");
    net_connect_ppp_shutdown();
    ret = esp_unregister_shutdown_handler(&net_connect_ppp_shutdown);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ESP_LOGI(TAG, "PPP interface deinitialized");
#endif
    ESP_LOGI(TAG, "All network interfaces disconnected");
    return ESP_OK;
}
