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

//void _mdns_browse_item_free(mdns_browse_t *browse);
esp_err_t _mdns_send_browse_action(mdns_action_type_t type, mdns_browse_t *browse);
esp_err_t _mdns_sync_browse_action(mdns_action_type_t type, mdns_browse_sync_t *browse_sync);
void _mdns_browse_sync(mdns_browse_sync_t *browse_sync);
void _mdns_browse_finish(mdns_browse_t *browse);
void _mdns_browse_add(mdns_browse_t *browse);
void _mdns_browse_send(mdns_browse_t *browse, mdns_if_t interface);
void mdns_browse_free(void);

void _mdns_browse_item_free(mdns_browse_t *browse);
mdns_browse_t *_mdns_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

void mdns_browse_send_all(mdns_if_t mdns_if);

#ifdef __cplusplus
}
#endif
