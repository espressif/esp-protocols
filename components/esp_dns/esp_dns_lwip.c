/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_dns_lwip.c
 * @brief Custom DNS module for ESP32 with multiple protocol support
 *
 * Provides DNS resolution capabilities with support for various protocols:
 * - Standard UDP/TCP DNS (Port 53)
 * - DNS over TLS (DoT) (Port 853)
 * - DNS over HTTPS (DoH) (Port 443)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "esp_dns.h"
#include "esp_dns_priv.h"

#define TAG "ESP_DNS_LWIP"

/* Global DNS handle instance */
extern esp_dns_handle_t g_dns_handle;

/* ========================= LWIP HOOK FUNCTIONS ========================= */

#if defined(CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM)
/**
 * @brief Custom DNS resolution hook for lwIP network connections
 *
 * @param name Hostname to resolve
 * @param addr Pointer to store resolved IP address
 * @param addrtype Type of address to resolve (IPv4/IPv6)
 * @param err Pointer to store error code
 *
 * @return int 0 if resolution should be handled by lwIP, 1 if handled by this module
 */
int lwip_hook_netconn_external_resolve(const char *name, ip_addr_t *addr, u8_t addrtype, err_t *err)
{
    if (g_dns_handle == NULL) {
        ESP_LOGD(TAG, "ESP_DNS module not initialized, resolving through native DNS");
        *err = ERR_OK;
        return 0;
    }

    if (name == NULL || addr == NULL || err == NULL) {
        if (err) {
            *err = ERR_ARG;
        }
        return 1;
    }

    /* Check if name is already an IP address */
    if (ipaddr_aton(name, addr)) {
        *err = ERR_OK;
        return 0;
    }

    /* Check if DNS server name matches or if it's localhost */
    if ((strcmp(name, g_dns_handle->config.dns_server) == 0) ||
#if LWIP_HAVE_LOOPIF
            (strcmp(name, "localhost") == 0) ||
#endif
            ipaddr_aton(name, addr)) {
        return 0;
    }

    u8_t rrtype;
    if ((addrtype == NETCONN_DNS_IPV4) || (addrtype == NETCONN_DNS_IPV4_IPV6)) {
        rrtype = DNS_RRTYPE_A;
    } else if ((addrtype == NETCONN_DNS_IPV6) || (addrtype == NETCONN_DNS_IPV6_IPV4)) {
        rrtype = DNS_RRTYPE_AAAA;
    } else {
        ESP_LOGE(TAG, "Invalid address type");
        *err = ERR_VAL;
        return 1;
    }

    /* Resolve based on configured transport type */
    switch (g_dns_handle->config.protocol) {
    case ESP_DNS_PROTOCOL_UDP:
        /* Return zero as lwIP DNS can handle UDP DNS */
        return 0;
    case ESP_DNS_PROTOCOL_TCP:
        *err = dns_resolve_tcp(g_dns_handle, name, addr, rrtype);
        break;
    case ESP_DNS_PROTOCOL_DOT:
        *err = dns_resolve_dot(g_dns_handle, name, addr, rrtype);
        break;
    case ESP_DNS_PROTOCOL_DOH:
        *err = dns_resolve_doh(g_dns_handle, name, addr, rrtype);
        break;
    default:
        ESP_LOGE(TAG, "Invalid transport type");
        *err = ERR_VAL;
    }

    return 1;
}
#else
#error "CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM is not defined. Please enable it in your menuconfig"
#endif /* CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM */
