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
 * @brief Check if a running browse consumes a service instance.
 *
 * @param instance Instance name.
 * @param service Service name.
 * @param proto Protocol name.
 * @return true if a matching running browse exists.
 *         Service and PTR record browses match all service instances; SRV and TXT record browses match only the named instance.
 */
bool mdns_priv_browse_has_service(const char *instance, const char *service, const char *proto);

/**
 * @brief Check whether a registered A/AAAA record browse consumes a hostname and address family.
 */
bool mdns_priv_browse_has_hostname(const char *hostname, mdns_browse_type_t type);

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
 *  @brief Looks for a running browse item as consumer of the provided record data
 *
 *  @note Called from the packet parser (mdns_receive.c)
 *
 *  @return matching browse item, or NULL if not found
 */
mdns_browse_t *mdns_priv_browse_find_consumer(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

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
 * @brief Build temporary results and synchronously notify matching browses from cache.
 *
 * @param entry Owner cache entry.
 * @param service Updated service cache, or NULL for hostname-level A/AAAA updates.
 * @param records Bitmask of cache record types to sync.
 *
 * @note Called from mdns_priv_cache_process_sync() in mdns_cache.c
 *
 * @return true if all matching browses have been notified about the updated service, false otherwise
 */
bool mdns_priv_browse_update_from_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                        mdns_cache_record_mask_t records);

/**
 * @brief Replay cached service records, or addresses when service is NULL, to the specified notifier on a browse.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND when no matching record is present,
 *         ESP_ERR_INVALID_ARG for invalid arguments, or the result projection error.
 */
esp_err_t mdns_priv_browse_notify_from_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                             mdns_browse_t *browse, const mdns_browse_notifier_t *notifier);

/**
 * @brief Notify matching record browses about goodbye or natural expiration.
 *
 * @param service Service cache, or NULL for hostname-level A/AAAA removal.
 * @param subtype The PTR subtype, or NULL for other records.
 * @param removed_record The record type removed.
 *
 * @note Service records are projected before their data is freed. Addresses are projected after
 *       removing the address from the list, but before freeing the owner entry, to report survivors.
 */
bool mdns_priv_browse_notify_record_removed_from_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                       const mdns_cache_subtype_t *subtype, mdns_cache_record_type_t removed_record);
#ifdef __cplusplus
}
#endif
