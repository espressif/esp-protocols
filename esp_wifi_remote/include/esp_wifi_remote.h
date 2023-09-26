/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_wifi.h"

// Public API
esp_err_t remote_esp_wifi_connect(void);
esp_err_t remote_esp_wifi_init(const wifi_init_config_t *config);
esp_err_t remote_esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t remote_esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf);
esp_err_t remote_esp_wifi_start(void);
esp_err_t remote_esp_wifi_stop(void);
esp_err_t remote_esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]);


// TODO: Move this to private include
// Private API
esp_err_t remote_esp_wifi_init_slave(void);

// handling channels
esp_err_t esp_wifi_remote_channel_rx(void *h, void *buffer, size_t len);
esp_err_t esp_wifi_remote_channel_set(wifi_interface_t ifx, void *h, esp_err_t (*tx_cb)(void *, void *, size_t));
esp_err_t esp_wifi_remote_rpc_channel_rx(void *h, void *buffer, size_t len);
esp_err_t esp_wifi_remote_rpc_channel_set(void *h, esp_err_t (*tx_cb)(void *, void *, size_t));
