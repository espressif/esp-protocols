/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_wifi.h"
//#include "wifi_remote_rpc_impl.hpp"
#include "eppp_link.h"

esp_netif_t *wifi_remote_eppp_init(eppp_type_t role)
{
    if (role == EPPP_SERVER) {
        eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
        config.transport = EPPP_TRANSPORT_UART;
        config.uart.tx_io = 22;  // d2
        config.uart.rx_io = 23;  // d3
        return eppp_open(role, &config, portMAX_DELAY);
    } else {
        eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
        config.transport = EPPP_TRANSPORT_UART;
        config.uart.tx_io = 17;
        config.uart.rx_io = 16;
        return eppp_open(role, &config, portMAX_DELAY);
    }
}
