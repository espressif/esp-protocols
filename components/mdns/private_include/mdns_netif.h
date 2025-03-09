/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "mdns_private.h"

#ifdef __cplusplus
extern "C" {
#endif

bool _mdns_if_is_dup(mdns_if_t tcpip_if);
void _mdns_disable_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
void _mdns_enable_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
mdns_if_t _mdns_get_other_if(mdns_if_t tcpip_if);
esp_err_t mdns_netif_init(void);
esp_err_t mdns_netif_deinit(void);
void unregister_predefined_handlers(void);
esp_err_t mdns_netif_free(void);
void _mdns_dup_interface(mdns_if_t tcpip_if);

#ifdef __cplusplus
}
#endif
