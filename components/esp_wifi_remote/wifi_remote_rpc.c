/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_remote.h"
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

esp_err_t remote_esp_wifi_connect(void)
{
    return esp_hosted_wifi_connect();
}

esp_err_t remote_esp_wifi_disconnect(void)
{
    return esp_hosted_wifi_disconnect();
}

esp_err_t remote_esp_wifi_init(const wifi_init_config_t *config)
{
    if (remote_esp_wifi_init_slave() != ESP_OK) {
        return ESP_FAIL;
    }
    return esp_hosted_wifi_init(config);
}

esp_err_t remote_esp_wifi_deinit(void)
{
    return esp_hosted_wifi_deinit();
}

esp_err_t remote_esp_wifi_set_mode(wifi_mode_t mode)
{
    return esp_hosted_wifi_set_mode(mode);
}

esp_err_t remote_esp_wifi_get_mode(wifi_mode_t *mode)
{
    return esp_hosted_wifi_get_mode(mode);
}

esp_err_t remote_esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
#if 0
    uint8_t *param = (uint8_t *)conf;
    uint32_t checksum = 0; // TODO: generate a random number and add it to both
    for (int i = 0; i < sizeof(wifi_config_t); ++i) {
        checksum += param[i];
    }

    // transmit the sensitive parameters over a secure channel
//    s_params_tx(s_params_channel, param, sizeof(wifi_config_t));

    // add only a checksum to the RPC
    return esp_hosted_wifi_set_config(interface, checksum);
#endif
    return esp_hosted_wifi_set_config(interface, conf);
}

esp_err_t remote_esp_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_hosted_wifi_get_config(interface, conf);
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
    return esp_hosted_wifi_get_mac(ifx, mac);
}

esp_err_t remote_esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6])
{
    return esp_hosted_wifi_set_mac(ifx, mac);
}

esp_err_t remote_esp_wifi_scan_start(const wifi_scan_config_t *config, bool block)
{
    return esp_hosted_wifi_scan_start(config, block);
}

esp_err_t remote_esp_wifi_scan_stop(void)
{
    return esp_hosted_wifi_scan_stop();
}

esp_err_t remote_esp_wifi_scan_get_ap_num(uint16_t *number)
{
    return esp_hosted_wifi_scan_get_ap_num(number);
}

esp_err_t remote_esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
    return esp_hosted_wifi_scan_get_ap_records(number, ap_records);
}

esp_err_t remote_esp_wifi_clear_ap_list(void)
{
    return esp_hosted_wifi_clear_ap_list();
}

esp_err_t remote_esp_wifi_restore(void)
{
    return esp_hosted_wifi_restore();
}

esp_err_t remote_esp_wifi_clear_fast_connect(void)
{
    return esp_hosted_wifi_clear_fast_connect();
}

esp_err_t remote_esp_wifi_deauth_sta(uint16_t aid)
{
    return esp_hosted_wifi_deauth_sta(aid);
}

esp_err_t remote_esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info)
{
    return esp_hosted_wifi_sta_get_ap_info(ap_info);
}

esp_err_t remote_esp_wifi_set_ps(wifi_ps_type_t type)
{
    return esp_hosted_wifi_set_ps(type);
}

esp_err_t remote_esp_wifi_get_ps(wifi_ps_type_t *type)
{
    return esp_hosted_wifi_get_ps(type);
}

esp_err_t remote_esp_wifi_set_storage(wifi_storage_t storage)
{
    return esp_hosted_wifi_set_storage(storage);
}

esp_err_t remote_esp_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw)
{
    return esp_hosted_wifi_set_bandwidth(ifx, bw);
}

esp_err_t remote_esp_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw)
{
    return esp_hosted_wifi_get_bandwidth(ifx, bw);
}

esp_err_t remote_esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second)
{
    return esp_hosted_wifi_set_channel(primary, second);
}

esp_err_t remote_esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second)
{
    return esp_hosted_wifi_get_channel(primary, second);
}

esp_err_t remote_esp_wifi_set_country_code(const char *country, bool ieee80211d_enabled)
{
    return esp_hosted_wifi_set_country_code(country, ieee80211d_enabled);
}

esp_err_t remote_esp_wifi_get_country_code(char *country)
{
    return esp_hosted_wifi_get_country_code(country);
}

esp_err_t remote_esp_wifi_set_country(const wifi_country_t *country)
{
    return esp_hosted_wifi_set_country(country);
}

esp_err_t remote_esp_wifi_get_country(wifi_country_t *country)
{
    return esp_hosted_wifi_get_country(country);
}

esp_err_t remote_esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta)
{
    return esp_hosted_wifi_ap_get_sta_list(sta);
}

esp_err_t remote_esp_wifi_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid)
{
    return esp_hosted_wifi_ap_get_sta_aid(mac, aid);
}

esp_err_t remote_esp_wifi_sta_get_rssi(int *rssi)
{
    return esp_hosted_wifi_sta_get_rssi(rssi);
}

esp_err_t remote_esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap)
{
    return esp_hosted_wifi_set_protocol(ifx, protocol_bitmap);
}

esp_err_t remote_esp_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap)
{
    return esp_hosted_wifi_get_protocol(ifx, protocol_bitmap);
}
