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
esp_err_t remote_esp_wifi_connect(void);
esp_err_t remote_esp_wifi_disconnect(void);
esp_err_t remote_esp_wifi_init(const wifi_init_config_t *config);
esp_err_t remote_esp_wifi_deinit(void);
esp_err_t remote_esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t remote_esp_wifi_get_mode(wifi_mode_t *mode);
esp_err_t remote_esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf);
esp_err_t remote_esp_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t remote_esp_wifi_start(void);
esp_err_t remote_esp_wifi_stop(void);
esp_err_t remote_esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]);
esp_err_t remote_esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6]);
esp_err_t remote_esp_wifi_scan_start(const wifi_scan_config_t *config, bool block);
esp_err_t remote_esp_wifi_scan_stop(void);
esp_err_t remote_esp_wifi_scan_get_ap_num(uint16_t *number);
esp_err_t remote_esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);
esp_err_t remote_esp_wifi_clear_ap_list(void);
esp_err_t remote_esp_wifi_restore(void);
esp_err_t remote_esp_wifi_clear_fast_connect(void);
esp_err_t remote_esp_wifi_deauth_sta(uint16_t aid);
esp_err_t remote_esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);
esp_err_t remote_esp_wifi_set_ps(wifi_ps_type_t type);
esp_err_t remote_esp_wifi_get_ps(wifi_ps_type_t *type);
esp_err_t remote_esp_wifi_set_storage(wifi_storage_t storage);
esp_err_t remote_esp_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw);
esp_err_t remote_esp_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw);
esp_err_t remote_esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
esp_err_t remote_esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second);
esp_err_t remote_esp_wifi_set_country_code(const char *country, bool ieee80211d_enabled);
esp_err_t remote_esp_wifi_get_country_code(char *country);
esp_err_t remote_esp_wifi_set_country(const wifi_country_t *country);
esp_err_t remote_esp_wifi_get_country(wifi_country_t *country);
esp_err_t remote_esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta);
esp_err_t remote_esp_wifi_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid);
esp_err_t remote_esp_wifi_sta_get_rssi(int *rssi);
esp_err_t remote_esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap);
esp_err_t remote_esp_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap);


// TODO: Move this to private include
// Private API
esp_err_t remote_esp_wifi_init_slave(void);

// handling channels
esp_err_t esp_wifi_remote_channel_rx(void *h, void *buffer, void *buff_to_free, size_t len);
esp_err_t esp_wifi_remote_channel_set(wifi_interface_t ifx, void *h, esp_remote_channel_tx_fn_t tx_cb);
esp_err_t esp_wifi_remote_rpc_channel_rx(void *h, void *buffer, size_t len);
esp_err_t esp_wifi_remote_rpc_channel_set(void *h, esp_remote_channel_tx_fn_t tx_cb);
