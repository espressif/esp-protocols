/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_hosted_api.h"
#include "esp_wifi_remote.h"
const char *TAG = "esp_remote_wifi_init";

esp_err_t remote_esp_wifi_init_slave(void)
{
    ESP_LOGI(TAG, "** %s **", __func__);
#if 0
    if (esp_hosted_setup() != ESP_OK) {
        return ESP_FAIL;
    }
#endif
    esp_remote_channel_tx_fn_t tx_cb;
    esp_remote_channel_t ch;

    // Add an RPC channel with default config (i.e. secure=true)
    struct esp_remote_channel_config config = ESP_HOSTED_CHANNEL_CONFIG_DEFAULT();
    config.if_type = ESP_SERIAL_IF;
    //TODO: add rpc channel from here
//ch = esp_hosted_add_channel(&config, &tx_cb, esp_wifi_remote_rpc_channel_rx);
//   esp_wifi_remote_rpc_channel_set(ch, tx_cb);

    // Add two other channels for the two WiFi interfaces (STA, softAP) in plain text
    config.secure = false;
    config.if_type = ESP_STA_IF;
    ch = esp_hosted_add_channel(&config, &tx_cb, esp_wifi_remote_channel_rx);
    esp_wifi_remote_channel_set(WIFI_IF_STA, ch, tx_cb);


    config.secure = false;
    config.if_type = ESP_AP_IF;
    ch = esp_hosted_add_channel(&config, &tx_cb, esp_wifi_remote_channel_rx);
    esp_wifi_remote_channel_set(WIFI_IF_AP, ch, tx_cb);

    return ESP_OK;
}
