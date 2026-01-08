/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "net_connect.h"
#include "net_connect_private.h"
#include "net_connect_wifi_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "sdkconfig.h"

#if CONFIG_NET_CONNECT_WIFI

static const char *TAG = "net_connect_wifi";
static esp_netif_t *s_example_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;
#if CONFIG_NET_CONNECT_IPV6
static SemaphoreHandle_t s_semph_get_ip6_addrs = NULL;
#endif

static int s_retry_num = 0;

/* Static storage for WiFi STA configuration and handle */
static net_wifi_sta_config_t s_wifi_sta_config;
static net_iface_handle_t s_wifi_sta_handle = NULL;
static bool s_wifi_sta_configured = false;

static void example_handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                                               int32_t event_id, void *event_data)
{
    s_retry_num++;
    int max_retry = s_wifi_sta_configured ? s_wifi_sta_config.max_retry : CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY;
    if (s_retry_num > max_retry) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        /* let net_connect_wifi_sta_do_connect() return */
        if (s_semph_get_ip_addrs) {
            xSemaphoreGive(s_semph_get_ip_addrs);
        }
#if CONFIG_NET_CONNECT_IPV6
        if (s_semph_get_ip6_addrs) {
            xSemaphoreGive(s_semph_get_ip6_addrs);
        }
#endif
        net_connect_wifi_sta_do_disconnect();
        return;
    }
    wifi_event_sta_disconnected_t *disconn = event_data;
    if (disconn->reason == WIFI_REASON_ROAMING) {
        ESP_LOGD(TAG, "station roaming, do nothing");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected %d, trying to reconnect...", disconn->reason);
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

static void example_handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                                            int32_t event_id, void *event_data)
{
#if CONFIG_NET_CONNECT_IPV6
    if (esp_netif != NULL) {
        esp_netif_create_ip6_linklocal(esp_netif);
    }
#endif // CONFIG_NET_CONNECT_IPV6
}

static void example_handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                                          int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!net_connect_is_our_netif(NET_CONNECT_NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}

#if CONFIG_NET_CONNECT_IPV6
static void example_handler_on_sta_got_ipv6(void *arg, esp_event_base_t event_base,
                                            int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    if (!net_connect_is_our_netif(NET_CONNECT_NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    ESP_LOGI(TAG, "Got IPv6 event: Interface \"%s\" address: " IPV6STR ", type: %s", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip), net_connect_ipv6_addr_types_to_str[ipv6_type]);

    if (ipv6_type == NET_CONNECT_PREFERRED_IPV6_TYPE) {
        if (s_semph_get_ip6_addrs) {
            xSemaphoreGive(s_semph_get_ip6_addrs);
        } else {
            ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %s", IPV62STR(event->ip6_info.ip), net_connect_ipv6_addr_types_to_str[ipv6_type]);
        }
    }
}
#endif // CONFIG_NET_CONNECT_IPV6

/**
 * @brief Populate WiFi config from Kconfig defaults
 */
static void populate_config_from_kconfig(net_wifi_sta_config_t *config)
{
    memset(config, 0, sizeof(net_wifi_sta_config_t));

#if !CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
    strncpy(config->ssid, CONFIG_NET_CONNECT_WIFI_SSID, sizeof(config->ssid) - 1);
    strncpy(config->password, CONFIG_NET_CONNECT_WIFI_PASSWORD, sizeof(config->password) - 1);
#endif

    config->scan_method = NET_CONNECT_WIFI_SCAN_METHOD;
    config->sort_method = NET_CONNECT_WIFI_CONNECT_AP_SORT_METHOD;
    config->threshold_rssi = CONFIG_NET_CONNECT_WIFI_SCAN_RSSI_THRESHOLD;
    config->auth_mode_threshold = NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    config->max_retry = CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY;

    /* IP configuration defaults */
    config->ip.use_dhcp = NET_CONNECT_DEFAULT_USE_DHCP;
    config->ip.enable_ipv6 = NET_CONNECT_DEFAULT_ENABLE_IPV6;
}

/**
 * @brief Convert net_wifi_sta_config_t to wifi_config_t
 */
static void convert_to_wifi_config(const net_wifi_sta_config_t *net_config, wifi_config_t *wifi_config)
{
    memset(wifi_config, 0, sizeof(wifi_config_t));

    strncpy((char*)wifi_config->sta.ssid, net_config->ssid, sizeof(wifi_config->sta.ssid) - 1);
    wifi_config->sta.ssid[sizeof(wifi_config->sta.ssid) - 1] = '\0';

    strncpy((char*)wifi_config->sta.password, net_config->password, sizeof(wifi_config->sta.password) - 1);
    wifi_config->sta.password[sizeof(wifi_config->sta.password) - 1] = '\0';

    wifi_config->sta.scan_method = net_config->scan_method;
    wifi_config->sta.sort_method = net_config->sort_method;
    wifi_config->sta.threshold.rssi = net_config->threshold_rssi;
    wifi_config->sta.threshold.authmode = net_config->auth_mode_threshold;
}

esp_err_t net_connect_wifi_start(void)
{
    if (s_example_sta_netif != NULL) {
        ESP_LOGD(TAG, "WiFi already started, skipping initialization");
        return ESP_OK;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    bool wifi_already_init = (ret == ESP_ERR_INVALID_STATE);

    if (!wifi_already_init) {
        ESP_ERROR_CHECK(ret);
    } else {
        ESP_LOGD(TAG, "WiFi already initialized, skipping esp_wifi_init()");
    }

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    esp_netif_config.if_desc = NET_CONNECT_NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    s_example_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    if (s_example_sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi netif (memory allocation failure)");
        if (!wifi_already_init) {
            esp_wifi_deinit();
        }
        return ESP_ERR_NO_MEM;
    }
    esp_wifi_set_default_wifi_sta_handlers();

    if (!wifi_already_init) {
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    return ESP_OK;
}


void net_connect_wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    if (s_example_sta_netif != NULL) {
        ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_sta_netif));
        esp_netif_destroy(s_example_sta_netif);
        s_example_sta_netif = NULL;
    }
}

/**
 * @brief Clean up event handlers and semaphores registered during WiFi connection attempt
 * @param cleanup_semaphores If true, also delete semaphores created for waiting
 */
static void wifi_connect_cleanup_handlers(bool cleanup_semaphores)
{
    /* Unregister event handlers */
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &example_handler_on_sta_got_ip);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &example_handler_on_wifi_connect);
#if CONFIG_NET_CONNECT_IPV6
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &example_handler_on_sta_got_ipv6);
#endif

    /* Clean up semaphores if requested */
    if (cleanup_semaphores) {
#if CONFIG_NET_CONNECT_IPV4
        if (s_semph_get_ip_addrs) {
            vSemaphoreDelete(s_semph_get_ip_addrs);
            s_semph_get_ip_addrs = NULL;
        }
#endif
#if CONFIG_NET_CONNECT_IPV6
        if (s_semph_get_ip6_addrs) {
            vSemaphoreDelete(s_semph_get_ip6_addrs);
            s_semph_get_ip6_addrs = NULL;
        }
#endif
    }
}

esp_err_t net_connect_wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    if (wait) {
#if CONFIG_NET_CONNECT_IPV4
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
#endif
#if CONFIG_NET_CONNECT_IPV6
        s_semph_get_ip6_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip6_addrs == NULL) {
#if CONFIG_NET_CONNECT_IPV4
            vSemaphoreDelete(s_semph_get_ip_addrs);
            s_semph_get_ip_addrs = NULL;
#endif
            return ESP_ERR_NO_MEM;
        }
#endif
    }
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &example_handler_on_sta_got_ip, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &example_handler_on_wifi_connect, s_example_sta_netif));
#if CONFIG_NET_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &example_handler_on_sta_got_ipv6, NULL));
#endif

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        wifi_connect_cleanup_handlers(wait);
        return ret;
    }
    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP(s)");
#if CONFIG_NET_CONNECT_IPV4
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
        vSemaphoreDelete(s_semph_get_ip_addrs);
        s_semph_get_ip_addrs = NULL;
#endif
#if CONFIG_NET_CONNECT_IPV6
        xSemaphoreTake(s_semph_get_ip6_addrs, portMAX_DELAY);
        vSemaphoreDelete(s_semph_get_ip6_addrs);
        s_semph_get_ip6_addrs = NULL;
#endif
        int max_retry = s_wifi_sta_configured ? s_wifi_sta_config.max_retry : CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY;
        if (s_retry_num > max_retry) {
            wifi_connect_cleanup_handlers(false);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t net_connect_wifi_sta_do_disconnect(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &example_handler_on_sta_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &example_handler_on_wifi_connect));
#if CONFIG_NET_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &example_handler_on_sta_got_ipv6));
#endif
    return esp_wifi_disconnect();
}

void net_connect_wifi_shutdown(void)
{
    ESP_LOGI(TAG, "WiFi shutdown handler called");
    net_connect_wifi_sta_do_disconnect();
    net_connect_wifi_stop();
    s_wifi_sta_configured = false;
    s_wifi_sta_handle = NULL;
}

net_iface_handle_t net_configure_wifi_sta(net_wifi_sta_config_t *config)
{
    ESP_LOGI(TAG, "Configuring Wi-Fi STA interface...");

    if (config == NULL) {
        /* Use Kconfig defaults */
        populate_config_from_kconfig(&s_wifi_sta_config);
    } else {
        /* Validate config */
        if (strlen(config->ssid) == 0) {
            ESP_LOGE(TAG, "STA SSID is empty");
            return NULL;
        }

        /* Store configuration */
        memcpy(&s_wifi_sta_config, config, sizeof(net_wifi_sta_config_t));
    }

    s_wifi_sta_configured = true;
    s_wifi_sta_handle = (net_iface_handle_t)&s_wifi_sta_config;

    return s_wifi_sta_handle;
}

esp_err_t net_connect_wifi(void)
{
    ESP_LOGI(TAG, "Connecting configured Wi-Fi interfaces...");

    if (!s_wifi_sta_configured) {
        ESP_LOGE(TAG, "Wi-Fi STA not configured. Call net_configure_wifi_sta() first");
        return ESP_ERR_INVALID_STATE;
    }

    /* Start WiFi driver and create netif */
    esp_err_t err = net_connect_wifi_start();
    if (err != ESP_OK) {
        return err;
    }

    /* Convert stored config to wifi_config_t */
    wifi_config_t wifi_config;
    convert_to_wifi_config(&s_wifi_sta_config, &wifi_config);

#if CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
    net_configure_stdin_stdout();
    char buf[sizeof(wifi_config.sta.ssid) + sizeof(wifi_config.sta.password) + 2] = {0};
    ESP_LOGI(TAG, "Please input ssid password:");
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        ESP_LOGE(TAG, "Failed to read SSID/password from stdin (EOF or error)");
        net_connect_wifi_stop();
        return ESP_ERR_INVALID_STATE;
    }
    buf[strcspn(buf, "\n")] = '\0'; /* removes '\n' if present */
    memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));

    char *rest = NULL;
    char *temp = strtok_r(buf, " ", &rest);
    if (temp == NULL) {
        ESP_LOGE(TAG, "SSID is empty or invalid");
        net_connect_wifi_stop();
        return ESP_ERR_INVALID_ARG;
    }
    strncpy((char*)wifi_config.sta.ssid, temp, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
    temp = strtok_r(NULL, " ", &rest);
    if (temp) {
        strncpy((char*)wifi_config.sta.password, temp, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
#endif

    /* Connect using existing internal function */
    err = net_connect_wifi_sta_do_connect(wifi_config, true);
    if (err != ESP_OK) {
        net_connect_wifi_stop();
        return err;
    }

    return ESP_OK;
}

esp_err_t net_disconnect_wifi(void)
{
    ESP_LOGI(TAG, "Disconnecting Wi-Fi interfaces...");

    net_connect_wifi_shutdown();

    return ESP_OK;
}

bool net_connect_wifi_is_configured(void)
{
    return s_wifi_sta_configured;
}

#endif /* CONFIG_NET_CONNECT_WIFI */
