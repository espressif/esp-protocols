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
 * @brief Check if a running browse `_service._proto` with the given subtype exists.
 *
 * @param service Service name.
 * @param proto Protocol name.
 * @param subtype Subtype name, or NULL to match browses without subtype.
 * @return true if a running browse with identical service, proto and subtype exists, false otherwise.
 */
bool mdns_priv_browse_has_subtype(const char *service, const char *proto, const char *subtype);

/**
 *  @brief Looks for the name/type in active browse items
 *
 *  @note Called from the packet parser (mdns_receive.c)
 *
 *  @return matching browse item, or NULL if not found
 */
mdns_browse_t *mdns_priv_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

/**
 * @brief Looks for an active browse matching a PTR owner name (service._proto.local, or with subtype _subtype._sub.service._proto.local)
 *
 * @param name PTR owner name.
 * @return matching browse item, or NULL if not found
 *
 * @note Called from the packet parser (mdns_receive.c)
 */
mdns_browse_t *mdns_priv_browse_find_ptr(mdns_name_t *name);

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
 * @brief Notify the affected normal/subtype browse about a PTR goodbye.
 *
 * @param subtype The PTR subtype, or NULL for the base PTR.
 *
 * @note Used for both goodbye and natural expiration.
 *
 * @note Must be called before the PTR/subtype is removed from the cache to avoid UAF.
 */
bool mdns_priv_browse_notify_ptr_goodbye_from_service_cache(const mdns_cache_entry_t *entry,
                                                            const mdns_service_cache_t *service,
                                                            const char *subtype);

/**
 * @brief Free all record browses.
 */
void mdns_priv_browse_record_free(void);

/**
 * @brief Perform action from mdns service queue
 *
 * @note Called by `free_action()` and `execute_action()` in mdns_service.c
 *
 * @param action Pointer to the action to perform.
 * @param type Type of the action.
 */
void mdns_priv_browse_record_action(mdns_action_t *action, mdns_action_subtype_t type);

/**
 * @brief Find a running record browse by service, protocol, and possible subtype.
 *
 * @param service Service name.
 * @param proto Protocol name.
 * @param subtype Subtype name. NULL if no subtype is required.
 * @param type Type of the record browse.
 * @return Pointer to the record browse if found, NULL otherwise.
 */
mdns_browse_record_t *mdns_priv_browse_record_find(const char *service, const char *proto, const char *subtype,
                                                   mdns_browse_record_type_t type);

/**
 * @brief Check if a running PTR record browse exists for `_service._proto`.
 *
 * @param service Service name.
 * @param proto Protocol name.
 * @return true if a running record browse exists, false otherwise.
 */
bool mdns_priv_browse_record_has_service(const char *service, const char *proto);

/**
 * @brief Find a running PTR record browse to store additional SRV, TXT, ADDR records.
 *
 * @note Unlike mdns_priv_browse_record_find(), this checks cache admission without
 *       matching subtype. SRV/TXT match service and protocol; addresses match the cached host.
 *
 * @return Pointer to the record browse if found, NULL otherwise.
 */
mdns_browse_record_t *mdns_priv_browse_record_find_ptr_cache_consumer(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if,
                                                                      mdns_ip_protocol_t ip_protocol);

/**
 * @brief Update the record browse from the service cache.
 *
 * @param entry Pointer to the cache entry.
 * @param service Pointer to the service cache.
 * @param record_mask Bitmask of records to update.
 * @return Bitmask of records that were updated.
 */
mdns_cache_record_mask_t mdns_priv_browse_record_update_from_service_cache(const mdns_cache_entry_t *entry,
                                                                           const mdns_service_cache_t *service,
                                                                           mdns_cache_record_mask_t record_mask);

/**
 * @brief Notify the record browse from the service cache.
 *
 * @param entry Pointer to the cache entry.
 * @param service Pointer to the service cache.
 * @param browse_record Pointer to the record browse.
 * @return true if successfully notified, false otherwise.
 */
bool mdns_priv_browse_record_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                       mdns_browse_record_t *browse_record);

/**
 * @brief Notify the record browse from the service cache for a goodbye message.
 *
 * @param entry Pointer to the cache entry.
 * @param service Pointer to the service cache.
 * @param record_mask Bitmask of records to update.
 * @param subtype Subtype name. NULL if no subtype is required.
 * @return true if successfully notified, false otherwise.
 */
bool mdns_priv_browse_record_notify_goodbye_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                               mdns_cache_record_mask_t record_mask, const char *subtype);

#ifdef __cplusplus
}
#endif
