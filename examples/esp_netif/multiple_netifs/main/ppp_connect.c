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
#include "esp_modem_api.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "iface_info.h"

struct ppp_info_t {
    iface_info_t parent;
    esp_modem_dce_t *dce;
    bool stop_task;
};

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


static void teardown_ppp(iface_info_t *info)
{
    struct ppp_info_t *ppp_info = __containerof(info, struct ppp_info_t, parent);

    esp_netif_action_disconnected(ppp_info->parent.netif, 0, 0, 0);
    esp_netif_action_stop(ppp_info->parent.netif, 0, 0, 0);
    esp_err_t err = esp_modem_set_mode(ppp_info->dce, ESP_MODEM_MODE_COMMAND);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
        return;
    }
    esp_modem_destroy(ppp_info->dce);
    vEventGroupDelete(event_group);
    ppp_info->stop_task = true;
    free(info);
}

static void ppp_task(void *args)
{
    struct ppp_info_t *ppp_info = args;
    int backoff_time = 15000;
    const int max_backoff = 60000;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;

    ppp_info->dce = esp_modem_new(&dte_config, &dce_config, ppp_info->parent.netif);

    int rssi, ber;
    esp_err_t ret = esp_modem_get_signal_quality(ppp_info->dce, &rssi, &ber);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
        goto failed;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    ret = esp_modem_set_mode(ppp_info->dce, ESP_MODEM_MODE_DATA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", ret);
        goto failed;
    }

failed:

#define CONTINUE_LATER() backoff_time *= 2;             \
                         if (backoff_time > max_backoff) { backoff_time = max_backoff; } \
                         continue;

    // now let's keep retrying
    while (!ppp_info->stop_task) {
        vTaskDelay(pdMS_TO_TICKS(backoff_time));
        if (ppp_info->parent.connected) {
            backoff_time = 5000;
            continue;
        }
        // try if the modem got stuck in data mode
        ESP_LOGI(TAG, "Trying to Sync with modem");
        ret = esp_modem_sync(ppp_info->dce);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Switching to command mode");
            esp_modem_set_mode(ppp_info->dce, ESP_MODEM_MODE_COMMAND);
            ESP_LOGI(TAG, "Retry sync 3 times");
            for (int i = 0; i < 3; ++i) {
                ret = esp_modem_sync(ppp_info->dce);
                if (ret == ESP_OK) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            if (ret != ESP_OK) {
                CONTINUE_LATER();
            }
        }
        ESP_LOGI(TAG, "Manual hang-up before reconnecting");
        ret = esp_modem_at(ppp_info->dce, "ATH", NULL, 2000);
        if (ret != ESP_OK) {
            CONTINUE_LATER();
        }
        ret = esp_modem_get_signal_quality(ppp_info->dce, &rssi, &ber);
        if (ret != ESP_OK) {
            CONTINUE_LATER();
        }
        ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
        ret = esp_modem_set_mode(ppp_info->dce, ESP_MODEM_MODE_DATA);
        if (ret != ESP_OK) {
            CONTINUE_LATER();
        }
    }

#undef CONTINUE_LATER

    vTaskDelete(NULL);
}

iface_info_t *setup_ppp(int prio)
{
    struct ppp_info_t *ppp_info = calloc(1, sizeof(struct ppp_info_t));
    assert(ppp_info);
    ppp_info->parent.teardown = teardown_ppp;
    ppp_info->parent.name = "Modem";
    event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, ppp_info));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, ppp_info));

    esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
    base_netif_cfg.route_prio = prio;
    esp_netif_config_t netif_ppp_config = { .base = &base_netif_cfg,
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

    teardown_ppp(&ppp_info->parent);
    return NULL;
}
