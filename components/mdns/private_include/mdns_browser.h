/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "mdns_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  Free browse item queue
 *
 *  @note Called from mdns_free()
 */
void mdns_priv_browse_free(void);

/**
 * @brief Check if a running browse `_service._proto` exists.
 *
 * @param service Service name.
 * @param proto Protocol name.
 * @return true if a running browse `_service._proto` exists, false otherwise.
 */
bool mdns_priv_browse_has_service(const char *service, const char *proto);

/**
 *  @brief Looks for the name/type in active browse items
 *
 *  @note Called from the packet parser (mdns_receive.c)
 *
 *  @return matching browse item, or NULL if not found
 */
mdns_browse_t *mdns_priv_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

/**
 * @brief Looks for an active browse matching a PTR owner name (service._proto.local)
 *
 * @note Called from the packet parser (mdns_receive.c)
 */
mdns_browse_t *mdns_priv_browse_find_ptr(mdns_name_t *name);

/**
 * @brief Allocate a browse sync object, or return @p sync when provided
 *
 * @note Currently this function is called to create a browse sync object
 *       for each temporary browse result to carry the result to be synced.
 *       The sync object will be immediately queued for notification and freed after notification.
 */
mdns_browse_sync_t *mdns_priv_browse_ensure_sync(mdns_browse_t *browse, mdns_browse_sync_t *sync);

/**
 * @brief Free browse sync object and its pending result list
 */
void mdns_priv_browse_sync_free(mdns_browse_sync_t *browse_sync);

/**
 * @brief Send out all browse queries
 *
 * @note Called from the network events (mdns_netif.c)
 * @note Calls (indirectly) search-send from mdns_querier.c, which sends out the query
 */
void mdns_priv_browse_send_all(mdns_if_t mdns_if);

/**
 * @brief Send out browse queries by IP protocol
 *
 * @note Called from the network events (mdns_netif.c)
 * @note Calls (indirectly) search-send from mdns_querier.c, which sends out the query
 */
void mdns_priv_browse_send_by_ip_protocol(mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol);

/**
 * @brief Sync browse results
 *
 * @note Called from the packet parser
 * @note Queue a sync action for the browse sync object
 */
esp_err_t mdns_priv_browse_sync(mdns_browse_sync_t *browse_sync);

/**
 * @brief Perform action from mdns service queue
 *
 * @note Called by `free_action()` and `execute_action()` in mdns_service.c
 */
void mdns_priv_browse_action(mdns_action_t *action, mdns_action_subtype_t type);

#ifdef __cplusplus
}
#endif
