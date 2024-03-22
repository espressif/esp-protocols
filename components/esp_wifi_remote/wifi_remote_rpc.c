/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_remote_private.h"
#include "esp_hosted_api.h"

static esp_remote_channel_t s_params_channel;
static esp_remote_channel_tx_fn_t s_params_tx;
static wifi_config_t s_last_wifi_conf;

esp_err_t esp_wifi_remote_rpc_channel_rx(void *h, void *buffer, size_t len)
{
    if (h == s_params_channel && len == sizeof(s_last_wifi_conf)) {
        memcpy(&s_last_wifi_conf, buffer, len); // TODO: use queue
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t esp_wifi_remote_rpc_channel_set(void *h, esp_err_t (*tx_cb)(void *, void *, size_t))
{
    s_params_channel = h;
    s_params_tx = tx_cb;
    return ESP_OK;
}
