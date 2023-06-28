/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* PPPoS Initialization

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "iface_info.h"
#include "ppp_connect.h"

static const int CONNECT_BIT = BIT0;
static const char *TAG = "pppos_connect";
static EventGroupHandle_t event_group = NULL;


static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        struct ppp_info_t *ppp_info = arg;
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
        ppp_info->parent.connected = false;
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
    struct ppp_info_t *ppp_info = arg;

    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        for (int i = 0; i < 2; ++i) {
            esp_netif_get_dns_info(ppp_info->parent.netif, i, &ppp_info->parent.dns[i]);
            ESP_LOGI(TAG, "DNS %i:" IPSTR, i, IP2STR(&ppp_info->parent.dns[i].ip.u_addr.ip4));
        }
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);
        ppp_info->parent.connected = true;

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        ppp_info->parent.connected = false;
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}


static void ppp_destroy(iface_info_t *info)
{
    struct ppp_info_t *ppp_info = __containerof(info, struct ppp_info_t, parent);

    esp_netif_action_disconnected(ppp_info->parent.netif, 0, 0, 0);
    esp_netif_action_stop(ppp_info->parent.netif, 0, 0, 0);
    ppp_destroy_context(ppp_info);
    vEventGroupDelete(event_group);
    ppp_info->stop_task = true;
    free(info);
}

iface_info_t *init_ppp(int prio)
{
    struct ppp_info_t *ppp_info = calloc(1, sizeof(struct ppp_info_t));
    assert(ppp_info);
    ppp_info->parent.destroy = ppp_destroy;
    ppp_info->parent.name = "Modem";
    event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, ppp_info));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, ppp_info));

    esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
    base_netif_cfg.route_prio = prio;
    esp_netif_config_t netif_ppp_config = { .base = &base_netif_cfg,
                                            .driver = ppp_driver_cfg,
                                            .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP
                                          };

    ppp_info->parent.netif = esp_netif_new(&netif_ppp_config);
    if (ppp_info->parent.netif == NULL) {
        goto err;
    }
    if (xTaskCreate(ppp_task, "ppp_retry_task", 4096, ppp_info, 5, NULL) != pdTRUE) {
        goto err;
    }

    ESP_LOGI(TAG, "Waiting for IP address");
    xEventGroupWaitBits(event_group, CONNECT_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    return &ppp_info->parent;

err:

    ppp_destroy(&ppp_info->parent);
    return NULL;
}
