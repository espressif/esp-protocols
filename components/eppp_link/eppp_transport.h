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

esp_err_t eppp_check_connection(esp_netif_t *netif);
