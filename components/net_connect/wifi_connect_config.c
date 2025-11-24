/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "net_connect_wifi_config.h"
#include "net_connect_private.h"
#include "net_connect.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "sdkconfig.h"

#if CONFIG_NET_CONNECT_CONNECT_WIFI

static const char *TAG = "net_connect_wifi_config";

/* Static storage for WiFi STA configuration and handle */
static net_wifi_sta_config_t s_wifi_sta_config;
static net_iface_handle_t s_wifi_sta_handle = NULL;
static bool s_wifi_sta_configured = false;

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
    net_connect_wifi_start();
    
    /* Convert stored config to wifi_config_t */
    wifi_config_t wifi_config;
    convert_to_wifi_config(&s_wifi_sta_config, &wifi_config);
    
#if CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
    net_configure_stdin_stdout();
    char buf[sizeof(wifi_config.sta.ssid) + sizeof(wifi_config.sta.password) + 2] = {0};
    ESP_LOGI(TAG, "Please input ssid password:");
    fgets(buf, sizeof(buf), stdin);
    int len = strlen(buf);
    buf[len - 1] = '\0'; /* removes '\n' */
    memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));

    char *rest = NULL;
    char *temp = strtok_r(buf, " ", &rest);
    strncpy((char*)wifi_config.sta.ssid, temp, sizeof(wifi_config.sta.ssid));
    memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
    temp = strtok_r(NULL, " ", &rest);
    if (temp) {
        strncpy((char*)wifi_config.sta.password, temp, sizeof(wifi_config.sta.password));
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
#endif
    
    /* Connect using existing internal function */
    esp_err_t err = net_connect_wifi_sta_do_connect(wifi_config, true);
    if (err != ESP_OK) {
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

#endif /* CONFIG_NET_CONNECT_CONNECT_WIFI */

