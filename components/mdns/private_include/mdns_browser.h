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

void mdns_browse_free(void);

mdns_browse_t *_mdns_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

void mdns_browse_send_all(mdns_if_t mdns_if);

void mdns_browse_action(mdns_action_t *action, mdns_action_subtype_t type);

//esp_err_t _mdns_add_browse_result(mdns_browse_sync_t *sync_browse, mdns_result_t *r);;
void _mdns_browse_result_add_txt(mdns_browse_t *browse, const char *instance, const char *service, const char *proto,
                                 mdns_txt_item_t *txt, uint8_t *txt_value_len, size_t txt_count, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol,
                                 uint32_t ttl, mdns_browse_sync_t *out_sync_browse);
void _mdns_browse_result_add_ip(mdns_browse_t *browse, const char *hostname, esp_ip_addr_t *ip,
                                mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse);
void _mdns_browse_result_add_srv(mdns_browse_t *browse, const char *hostname, const char *instance, const char *service, const char *proto,
                                 uint16_t port, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse);
#ifdef __cplusplus
}
#endif
