/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once


#define ESP_WIFI_MAX_CONN_NUM  (15)       /**< max number of stations which can connect to ESP32/ESP32S3/ESP32S2 soft-AP */

/** @brief List of stations associated with the Soft-AP */
typedef struct wifi_sta_list_t {
    wifi_sta_info_t sta[ESP_WIFI_MAX_CONN_NUM]; /**< station list */
    int       num; /**< number of stations in the list (other entries are invalid) */
} wifi_sta_list_t;
