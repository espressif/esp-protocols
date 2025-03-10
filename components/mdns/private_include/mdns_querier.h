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

void _mdns_query_results_free(mdns_result_t *results);
void _mdns_search_add(mdns_search_once_t *search);
void _mdns_search_finish(mdns_search_once_t *search);
void _mdns_search_run(void);
void mdns_search_free(void);
void _mdns_search_finish_done(void);
void _mdns_search_send(mdns_search_once_t *search);
mdns_search_once_t *_mdns_search_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
mdns_search_once_t *_mdns_search_find_from(mdns_search_once_t *s, mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
void _mdns_search_free(mdns_search_once_t *search);

#ifdef __cplusplus
}
#endif
