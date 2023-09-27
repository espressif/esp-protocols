/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_wifi.h"
#include "esp_private/wifi_os_adapter.h"
#include "esp_log.h"
#include "esp_wifi_remote.h"

#define WEAK __attribute__((weak))

WEAK ESP_EVENT_DEFINE_BASE(WIFI_EVENT);

WEAK wifi_osi_funcs_t g_wifi_osi_funcs;
WEAK const wpa_crypto_funcs_t g_wifi_default_wpa_crypto_funcs;
WEAK uint64_t g_wifi_feature_caps;

WEAK esp_err_t esp_wifi_connect(void)
{
    return remote_esp_wifi_connect();
}

WEAK esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
    return remote_esp_wifi_init(config);
}

WEAK esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
    return remote_esp_wifi_set_mode(mode);
}

WEAK esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return remote_esp_wifi_set_config(interface, conf);
}

WEAK esp_err_t esp_wifi_start(void)
{
    return remote_esp_wifi_start();
}

WEAK esp_err_t esp_wifi_stop(void)
{
    return remote_esp_wifi_stop();
}

WEAK esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    return remote_esp_wifi_get_mac(ifx, mac);
}
