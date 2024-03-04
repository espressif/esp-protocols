/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_wifi.h"

typedef esp_err_t (*esp_remote_channel_rx_fn_t)(void *h, void *buffer, void *buff_to_free, size_t len);
typedef esp_err_t (*esp_remote_channel_tx_fn_t)(void *h, void *buffer, size_t len);

typedef struct esp_remote_channel *esp_remote_channel_t;
typedef struct esp_remote_channel_config *esp_remote_channel_config_t;

// Public API
#define ESP_WIFI_REMOTE_WITH_HOSTED 1
#if ESP_WIFI_REMOTE_WITH_HOSTED
#include "esp_wifi_remote_with_hosted.h"
#else
#include "esp_wifi_remote_api.h"
#endif

// TODO: Move this to private include
// Private API
esp_err_t remote_esp_wifi_init_slave(void);

// handling channels
esp_err_t esp_wifi_remote_channel_rx(void *h, void *buffer, void *buff_to_free, size_t len);
esp_err_t esp_wifi_remote_channel_set(wifi_interface_t ifx, void *h, esp_remote_channel_tx_fn_t tx_cb);
esp_err_t esp_wifi_remote_rpc_channel_rx(void *h, void *buffer, size_t len);
esp_err_t esp_wifi_remote_rpc_channel_set(void *h, esp_remote_channel_tx_fn_t tx_cb);
