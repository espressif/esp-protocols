/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_netif.h"
#include "NetworkInterface.h"

struct netif;

typedef NetworkInterface_t *(*init_fn_t)(BaseType_t xEMACIndex, NetworkInterface_t *pxInterface);
typedef esp_err_t (*input_fn_t)(NetworkInterface_t *pxInterface, void *buffer, size_t len, void *eb);

struct esp_netif_netstack_config {
    init_fn_t init_fn;
    input_fn_t input_fn;
};
