/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_dns_udp.h"
#include "lwip/dns.h"

#define TAG "ESP_DNS_UDP"



int init_udp_dns(esp_dns_handle_t handle)
{
    ESP_LOGI(TAG, "Initializing UDP DNS");

    /* TBD: Set the dns server here as it's the only thing we can control for udp for now */

    /* Return zero as all initializations of dns udp is handled by lwip dns */
    return 0;
}

int cleanup_udp_dns(esp_dns_handle_t handle)
{
    ESP_LOGI(TAG, "Cleaning up UDP DNS");

    /* Return zero as all deinitializations of dns udp is handled by lwip dns */
    return 0;
}

/* DNS resolution functions for different transport types */
err_t dns_resolve_udp(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype)
{
    // TODO: Implement UDP DNS resolution
    if (addr == NULL) {
        return ERR_ARG;
    }

    return ERR_OK;
}
