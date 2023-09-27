/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_remote.h"
#include "esp_hosted_api.h"

static esp_hosted_channel_t *s_params_channel;
static esp_hosted_channel_fn_t s_params_tx;
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
    s_params_channel =h;
    s_params_tx = tx_cb;
    return ESP_OK;
}

esp_err_t remote_esp_wifi_connect(void)
{
    return esp_hosted_wifi_connect();
}

esp_err_t remote_esp_wifi_init(const wifi_init_config_t *config)
{
    if (remote_esp_wifi_init_slave() != ESP_OK) {
        return ESP_FAIL;
    }
    return esp_hosted_wifi_init(config);
}

esp_err_t remote_esp_wifi_set_mode(wifi_mode_t mode)
{
    return esp_hosted_wifi_set_mode(mode);
}

esp_err_t remote_esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    uint8_t *param = (uint8_t*)conf;
    uint32_t checksum = 0; // TODO: generate a random number and add it to both
    for (int i=0; i<sizeof(wifi_config_t); ++i)
        checksum += param[i];

    // transmit the sensitive parameters over a secure channel
//    s_params_tx(s_params_channel, param, sizeof(wifi_config_t));

    // add only a checksum to the RPC
    return esp_hosted_wifi_set_config(interface, (wifi_config_t *)checksum);
}

esp_err_t remote_esp_wifi_start(void)
{
    return esp_hosted_wifi_start();
}

esp_err_t remote_esp_wifi_stop(void)
{
    return esp_hosted_wifi_stop();
}

esp_err_t remote_esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    return esp_hosted_wifi_get_mac_addr(ifx, mac);
}
