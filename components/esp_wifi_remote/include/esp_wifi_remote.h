/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_wifi.h"
#include "esp_wifi_remote_api.h"

typedef esp_err_t (*esp_remote_channel_rx_fn_t)(void *h, void *buffer, void *buff_to_free, size_t len);
typedef esp_err_t (*esp_remote_channel_tx_fn_t)(void *h, void *buffer, size_t len);

typedef struct esp_remote_channel *esp_remote_channel_t;
typedef struct esp_remote_channel_config *esp_remote_channel_config_t;

// TODO: Move this to private include
// Private API
