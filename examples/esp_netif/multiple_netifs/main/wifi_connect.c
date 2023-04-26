/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* WiFi connection

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "iface_info.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static const char *TAG = "wifi_connect";
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

static void event_handler(void *args, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    struct iface_info_t *wifi_info = args;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_info->connected = false;
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY || CONFIG_ESP_MAXIMUM_RETRY == 0) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;

        ESP_LOGI(TAG, "WiFi station Got IP Address");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");

        for (int i = 0; i < 2; ++i) {
            esp_netif_get_dns_info(wifi_info->netif, i, &wifi_info->dns[i]);
            ESP_LOGI(TAG, "DNS %i:" IPSTR, i, IP2STR(&wifi_info->dns[i].ip.u_addr.ip4));
        }
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_info->connected = true;
    }
}

static void teardown_wifi(iface_info_t *info)
{
    esp_netif_action_disconnected(info->netif, 0, 0, 0);
    esp_netif_action_stop(info->netif, 0, 0, 0);
    esp_wifi_stop();
    esp_wifi_deinit();
    free(info);
}

iface_info_t *setup_wifi(int prio)
{
    struct iface_info_t *wifi_info = malloc(sizeof(iface_info_t));
    assert(wifi_info);
    wifi_info->teardown = teardown_wifi;
    wifi_info->name = "WiFi station";
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.route_prio = prio;
    wifi_info->netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, wifi_info));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, wifi_info));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection or a failure (connection failed or a timeout) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, (WIFI_CONNECTED_BIT | WIFI_FAIL_BIT), pdFALSE, pdFALSE, pdMS_TO_TICKS(5000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
        teardown_wifi(wifi_info);
        wifi_info = NULL;
    } else if (CONFIG_ESP_MAXIMUM_RETRY == 0) {
        ESP_LOGI(TAG, "No connection at the moment, will keep retrying...");
    } else {
        ESP_LOGE(TAG, "Failed to connect withing specified timeout");
        teardown_wifi(wifi_info);
        wifi_info = NULL;
    }
    return wifi_info;
}
