/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_netif_types.h"

struct eppp_handle {
    esp_netif_driver_base_t base;
    eppp_type_t role;
    bool stop;
    bool exited;
    bool netif_stop;
};

//esp_err_t eppp_transport_init(eppp_config_t *config, esp_netif_t *esp_netif);
//
//esp_err_t eppp_transport_tx(void *h, void *buffer, size_t len);
//
//esp_err_t eppp_transport_rx(esp_netif_t *netif);
//
//void eppp_transport_deinit(void);
