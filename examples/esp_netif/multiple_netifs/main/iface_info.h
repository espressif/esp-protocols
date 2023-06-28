/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Common interface info

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once
#include "esp_netif.h"

struct iface_info_t {
    esp_netif_t *netif;
    esp_netif_dns_info_t dns[2];
    void (*destroy)(struct iface_info_t *);
    const char *name;
    bool connected;
};

typedef struct iface_info_t iface_info_t;
