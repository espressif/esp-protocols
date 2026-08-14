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
 * @brief Perform action from mdns service queue
 *
 * @note Called by `free_action()` and `execute_action()` in mdns_service.c
 */
void mdns_priv_browse_action(mdns_action_t *action, mdns_action_subtype_t type);

/**
 * @brief  Build temporary result and queue for sync from service cache
 *
 * @param entry Owner cache entry.
 * @param service Updated service cache.
 * @param records Bitmask of cache record types to sync.
 *
 * @note Called from mdns_priv_cache_process_sync() in mdns_cache.c
 *
 * @return true if all matching browses have been notified about the updated service, false otherwise
 */
bool mdns_priv_browse_update_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                mdns_cache_record_mask_t records);

/**
 * @brief Notify one browse about one visible cache service.
 *
 * @return true if browse is successfully notified, false otherwise
 */
bool mdns_priv_browse_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                mdns_browse_t *browse);

/**
 * @brief Notify the affected browse about a PTR goodbye.
 *
 * @note Must be called before the PTR service cache is removed to avoid UAF.
 */
bool mdns_priv_browse_notify_ptr_goodbye_from_service_cache(const mdns_cache_entry_t *entry,
                                                            const mdns_service_cache_t *service);
#ifdef __cplusplus
}
#endif
