/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <esp_private/wifi.h>
#include "esp_err.h"
#include <rpc_wrapper.h>

#define CHANNELS 2

static esp_hosted_channel_fn_t s_tx_cb[CHANNELS];
static esp_hosted_channel_t *s_channel[CHANNELS];
static wifi_rxcb_t s_rx_fn[CHANNELS];

esp_err_t esp_wifi_remote_channel_rx(void *h, void *buffer, size_t len)
{
    if (h == s_channel[0]) {
        return s_rx_fn[0](buffer, len, buffer);
    }
    if (h == s_channel[1]) {
        return s_rx_fn[1](buffer, len, buffer);
    }
    return ESP_FAIL;
}

esp_err_t esp_wifi_remote_channel_set(wifi_interface_t ifx, void *h, esp_err_t (*tx_cb)(void *, void *, size_t))
{
    if (ifx == WIFI_IF_STA) {
        s_channel[0] = h;
        s_tx_cb[0] = tx_cb;
        return ESP_OK;
    }
    if (ifx == WIFI_IF_AP) {
        s_channel[1] = h;
        s_tx_cb[1] = tx_cb;
        return ESP_OK;
    }
    return ESP_FAIL;
}


esp_err_t esp_wifi_internal_set_sta_ip(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_internal_reg_netstack_buf_cb(wifi_netstack_buf_ref_cb_t ref, wifi_netstack_buf_free_cb_t free)
{
    return ESP_OK;
}

void esp_wifi_internal_free_rx_buffer(void* buffer)
{
//    free(buffer);
}

int esp_wifi_internal_tx(wifi_interface_t ifx, void *buffer, uint16_t len)
{
    if (ifx == WIFI_IF_STA) {
        return s_tx_cb[0](s_channel[0], buffer, len);
    }
    if (ifx == WIFI_IF_AP) {
        return s_tx_cb[1](s_channel[1], buffer, len);
    }

    return -1;
}

esp_err_t esp_wifi_internal_reg_rxcb(wifi_interface_t ifx, wifi_rxcb_t fn)
{
    if (ifx == WIFI_IF_STA) {
        s_rx_fn[0] = fn;
        return ESP_OK;
    }
    if (ifx == WIFI_IF_AP) {
        s_rx_fn[1] = fn;
        return ESP_OK;
    }

    return ESP_FAIL;
}
