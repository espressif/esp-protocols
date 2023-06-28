/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
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
#include "ppp_connect.h"

static const char *TAG = "ppp_esp_modem";

const esp_netif_driver_ifconfig_t *ppp_driver_cfg = NULL;

void ppp_task(void *args)
{
    struct ppp_info_t *ppp_info = args;
    int backoff_time = 15000;
    const int max_backoff = 60000;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_PPP_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_PPP_UART_RX_PIN;

    esp_modem_dce_t *dce = esp_modem_new(&dte_config, &dce_config, ppp_info->parent.netif);
    ppp_info->context = dce;

    int rssi, ber;
    esp_err_t ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", ret, esp_err_to_name(ret));
        goto failed;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
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
        ret = esp_modem_sync(dce);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Switching to command mode");
            esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
            ESP_LOGI(TAG, "Retry sync 3 times");
            for (int i = 0; i < 3; ++i) {
                ret = esp_modem_sync(dce);
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
        ret = esp_modem_at(dce, "ATH", NULL, 2000);
        if (ret != ESP_OK) {
            CONTINUE_LATER();
        }
        ret = esp_modem_get_signal_quality(dce, &rssi, &ber);
        if (ret != ESP_OK) {
            CONTINUE_LATER();
        }
        ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
        ret = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
        if (ret != ESP_OK) {
            CONTINUE_LATER();
        }
    }

#undef CONTINUE_LATER

    vTaskDelete(NULL);
}

void ppp_destroy_context(struct ppp_info_t *ppp_info)
{
    esp_modem_dce_t *dce = ppp_info->context;
    esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
        return;
    }
    esp_modem_destroy(dce);
}
