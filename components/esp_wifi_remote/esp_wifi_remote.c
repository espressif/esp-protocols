/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_wifi_remote.h"

#define WEAK __attribute__((weak))

WEAK ESP_EVENT_DEFINE_BASE(WIFI_EVENT);

WEAK wifi_osi_funcs_t g_wifi_osi_funcs;
WEAK const wpa_crypto_funcs_t g_wifi_default_wpa_crypto_funcs;
WEAK uint64_t g_wifi_feature_caps =
#if CONFIG_ESP32_WIFI_ENABLE_WPA3_SAE
    CONFIG_FEATURE_WPA3_SAE_BIT |
#endif
#if CONFIG_SPIRAM
    CONFIG_FEATURE_CACHE_TX_BUF_BIT |
#endif
#if CONFIG_ESP_WIFI_FTM_INITIATOR_SUPPORT
    CONFIG_FEATURE_FTM_INITIATOR_BIT |
#endif
#if CONFIG_ESP_WIFI_FTM_RESPONDER_SUPPORT
    CONFIG_FEATURE_FTM_RESPONDER_BIT |
#endif
    0;

WEAK esp_err_t esp_wifi_connect(void)
{
    return remote_esp_wifi_connect();
}

WEAK esp_err_t esp_wifi_disconnect(void)
{
    return remote_esp_wifi_disconnect();
}

WEAK esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
    return remote_esp_wifi_init(config);
}

WEAK esp_err_t esp_wifi_deinit(void)
{
    return remote_esp_wifi_deinit();
}

WEAK esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
    return remote_esp_wifi_set_mode(mode);
}

WEAK esp_err_t esp_wifi_get_mode(wifi_mode_t *mode)
{
    return remote_esp_wifi_get_mode(mode);
}

WEAK esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return remote_esp_wifi_set_config(interface, conf);
}

WEAK esp_err_t esp_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return remote_esp_wifi_get_config(interface, conf);
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

WEAK esp_err_t esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6])
{
    return remote_esp_wifi_set_mac(ifx, mac);
}

WEAK esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block)
{
    return remote_esp_wifi_scan_start(config, block);
}

WEAK esp_err_t esp_wifi_scan_stop(void)
{
    return remote_esp_wifi_scan_stop();
}

WEAK esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number)
{
    return remote_esp_wifi_scan_get_ap_num(number);
}

WEAK esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
    return remote_esp_wifi_scan_get_ap_records(number, ap_records);
}

WEAK esp_err_t esp_wifi_clear_ap_list(void)
{
    return remote_esp_wifi_clear_ap_list();
}

WEAK esp_err_t esp_wifi_restore(void)
{
    return remote_esp_wifi_restore();
}

WEAK esp_err_t esp_wifi_clear_fast_connect(void)
{
    return remote_esp_wifi_clear_fast_connect();
}

WEAK esp_err_t esp_wifi_deauth_sta(uint16_t aid)
{
    return remote_esp_wifi_deauth_sta(aid);
}

WEAK esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info)
{
    return remote_esp_wifi_sta_get_ap_info(ap_info);
}

WEAK esp_err_t esp_wifi_set_ps(wifi_ps_type_t type)
{
    return remote_esp_wifi_set_ps(type);
}

WEAK esp_err_t esp_wifi_get_ps(wifi_ps_type_t *type)
{
    return remote_esp_wifi_get_ps(type);
}

WEAK esp_err_t esp_wifi_set_storage(wifi_storage_t storage)
{
    return remote_esp_wifi_set_storage(storage);
}

WEAK esp_err_t esp_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw)
{
    return remote_esp_wifi_set_bandwidth(ifx, bw);
}

WEAK esp_err_t esp_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw)
{
    return remote_esp_wifi_get_bandwidth(ifx, bw);
}

WEAK esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second)
{
    return remote_esp_wifi_set_channel(primary, second);
}

WEAK esp_err_t esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second)
{
    return remote_esp_wifi_get_channel(primary, second);
}

WEAK esp_err_t esp_wifi_set_country_code(const char *country, bool ieee80211d_enabled)
{
    return remote_esp_wifi_set_country_code(country, ieee80211d_enabled);
}

WEAK esp_err_t esp_wifi_get_country_code(char *country)
{
    return remote_esp_wifi_get_country_code(country);
}

WEAK esp_err_t esp_wifi_set_country(const wifi_country_t *country)
{
    return remote_esp_wifi_set_country(country);
}

WEAK esp_err_t esp_wifi_get_country(wifi_country_t *country)
{
    return remote_esp_wifi_get_country(country);
}

WEAK esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta)
{
    return remote_esp_wifi_ap_get_sta_list(sta);
}

WEAK esp_err_t esp_wifi_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid)
{
    return remote_esp_wifi_ap_get_sta_aid(mac, aid);
}

WEAK esp_err_t esp_wifi_sta_get_rssi(int *rssi)
{
    return remote_esp_wifi_sta_get_rssi(rssi);
}

WEAK esp_err_t esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap)
{
    return remote_esp_wifi_set_protocol(ifx, protocol_bitmap);
}

WEAK esp_err_t esp_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap)
{
    return remote_esp_wifi_get_protocol(ifx, protocol_bitmap);
}
