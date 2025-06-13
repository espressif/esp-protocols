/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_netif_types.h"
#include "sdkconfig.h"

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
#define NR_OF_CHANNELS CONFIG_EPPP_LINK_NR_OF_CHANNELS
#else
#define NR_OF_CHANNELS 1
#endif

struct eppp_handle {
    esp_netif_driver_base_t base;
    eppp_type_t role;
    bool stop;
    bool exited;
    bool netif_stop;
#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
    eppp_channel_fn_t channel_tx;
    eppp_channel_fn_t channel_rx;
    void* context;
#endif
};

esp_err_t eppp_check_connection(esp_netif_t *netif);
