/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

struct esp_wifi_remote_config {
    wifi_interface_t interface;
    wifi_config_t conf;
};

struct esp_wifi_remote_mac_t {
    esp_err_t err;
    uint8_t mac[6];
};


esp_err_t esp_wifi_remote_set_config(wifi_interface_t interface, wifi_config_t *conf);

esp_err_t esp_wifi_remote_set_mode(wifi_mode_t mode);

esp_err_t esp_wifi_remote_init(wifi_init_config_t *config);

esp_err_t esp_wifi_remote_start(void);

esp_err_t esp_wifi_remote_connect(void);

esp_err_t esp_wifi_remote_get_mac(wifi_interface_t ifx, uint8_t mac[6]);

#ifdef __cplusplus
}
#endif
