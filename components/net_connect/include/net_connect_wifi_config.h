/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"
#include <stdbool.h>
#include "net_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_NET_CONNECT_WIFI

/**
 * @brief Configure Wi-Fi Station (STA) interface.
 *
 * Stores STA configuration and returns a handle for later connection.
 * Passing NULL uses default/Kconfig settings.
 *
 * @param[in] config Pointer to Wi-Fi STA configuration (can be NULL)
 * @return Interface handle for STA interface, or NULL on error
 */
net_iface_handle_t net_configure_wifi_sta(net_wifi_sta_config_t *config);

/**
 * @brief Connect Wi-Fi interface using stored configuration.
 *
 * Connects the Wi-Fi STA interface using the configuration previously
 * stored via net_configure_wifi_sta().
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_STATE if WiFi is not configured
 *  - ESP_FAIL or error code on failure
 */
esp_err_t net_connect_wifi(void);

/**
 * @brief Disconnect Wi-Fi interface.
 *
 * Disconnects the Wi-Fi STA interface that was connected via net_connect_wifi().
 *
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL or error code on failure
 */
esp_err_t net_disconnect_wifi(void);

/**
 * @brief Check if WiFi is configured via new API.
 *
 * Internal function to check if WiFi was configured using the new API.
 *
 * @return true if configured, false otherwise
 */
bool net_connect_wifi_is_configured(void);

#endif /* CONFIG_NET_CONNECT_WIFI */

#ifdef __cplusplus
}
#endif
