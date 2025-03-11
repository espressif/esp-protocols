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

void _mdns_search_run(void);
void mdns_search_free(void);
void _mdns_query_results_free(mdns_result_t *results);
void _mdns_search_finish_done(void);
mdns_search_once_t *_mdns_search_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
mdns_search_once_t *_mdns_search_find_from(mdns_search_once_t *s, mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

mdns_ip_addr_t *_mdns_result_addr_create_ip(esp_ip_addr_t *ip);
void _mdns_search_result_add_txt(mdns_search_once_t *search, mdns_txt_item_t *txt, uint8_t *txt_value_len,
                                 size_t txt_count, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol,
                                 uint32_t ttl);
void _mdns_search_result_add_ip(mdns_search_once_t *search, const char *hostname, esp_ip_addr_t *ip,
                                mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl);
void _mdns_search_result_add_srv(mdns_search_once_t *search, const char *hostname, uint16_t port,
                                 mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl);
mdns_result_t *_mdns_search_result_add_ptr(mdns_search_once_t *search, const char *instance,
                                           const char *service_type, const char *proto, mdns_if_t tcpip_if,
                                           mdns_ip_protocol_t ip_protocol, uint32_t ttl);

void mdns_query_action(mdns_action_t *action, mdns_action_subtype_t type);

#ifdef __cplusplus
}
#endif
