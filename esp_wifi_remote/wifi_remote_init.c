/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include <rpc_wrapper.h>
#include "esp_wifi_remote.h"

esp_err_t remote_esp_wifi_init_slave(void)
{
    if (esp_hosted_setup() != ESP_OK) {
        return ESP_FAIL;
    }
    esp_hosted_channel_fn_t tx_cb;
    esp_hosted_channel_t * ch;

    // Add an RPC channel with default config (i.e. secure=true)
    esp_hosted_channel_config_t config = ESP_HOSTED_CHANNEL_CONFIG_DEFAULT();
    ch = esp_hosted_add_channel(&config, &tx_cb, esp_wifi_remote_rpc_channel_rx);
    esp_wifi_remote_rpc_channel_set(ch, tx_cb);

    // Add two other channels for the two WiFi interfaces (STA, softAP) in plain text
    config.secure = false;
    ch = esp_hosted_add_channel(&config, &tx_cb, esp_wifi_remote_channel_rx); // TODO: add checks for `ch` and `tx_cb`
    esp_wifi_remote_channel_set(WIFI_IF_STA, ch, tx_cb);
    ch = esp_hosted_add_channel(&config, &tx_cb, esp_wifi_remote_channel_rx);
    esp_wifi_remote_channel_set(WIFI_IF_AP, ch, tx_cb);

    return ESP_OK;
}
