/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_netif.h"
#include <stdlib.h>
#include <string.h>

esp_netif_t *esp_netif_new(const esp_netif_config_t *config)
{
    esp_netif_t *netif = (esp_netif_t *)calloc(1, sizeof(esp_netif_t));
    return netif;
}

void esp_netif_destroy(esp_netif_t *esp_netif)
{
    free(esp_netif);
}

int esp_netif_receive(esp_netif_t *netif, uint8_t *data, size_t len)
{
    return 0;
}

esp_ip6_addr_type_t esp_netif_ip6_get_addr_type(esp_ip6_addr_t *ip6_addr)
{
    return ESP_IP6_ADDR_IS_UNKNOWN;
}
