/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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
 * @brief Free all resolvers.
 */
void mdns_priv_resolver_free(void);

/**
 * @brief Perform action from mdns service queue
 *
 * @note Called by `free_action()` and `execute_action()` in mdns_service.c
 *
 * @param action Pointer to the action to perform.
 * @param type Type of the action.
 */
void mdns_priv_resolver_action(mdns_action_t *action, mdns_action_subtype_t type);

/**
 * @brief Find a running resolver by service, protocol, and possible subtype.
 *
 * @param service Service name.
 * @param proto Protocol name.
 * @param subtype Subtype name. NULL if no subtype is required.
 * @param type Type of the resolver.
 * @return Pointer to the resolver if found, NULL otherwise.
 */
mdns_resolver_t *mdns_priv_resolver_find(const char *service, const char *proto, const char *subtype,
                                         mdns_resolver_type_t type);

/**
 * @brief Check if a running PTR resolver exists for `_service._proto`.
 *
 * @param service Service name.
 * @param proto Protocol name.
 * @return true if a running resolver exists, false otherwise.
 */
bool mdns_priv_resolver_has_service(const char *service, const char *proto);

/**
 * @brief Find a running PTR resolver to store additional SRV, TXT, ADDR records.
 *
 * @return Pointer to the resolver if found, NULL otherwise.
 */
mdns_resolver_t *mdns_priv_resolver_find_ptr(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if,
                                             mdns_ip_protocol_t ip_protocol);

/**
 * @brief Update the resolver from the service cache.
 *
 * @param entry Pointer to the cache entry.
 * @param service Pointer to the service cache.
 * @param record_mask Bitmask of records to update.
 * @return Bitmask of records that were updated.
 */
mdns_cache_record_mask_t mdns_priv_resolver_update_from_service_cache(const mdns_cache_entry_t *entry,
                                                                      const mdns_service_cache_t *service,
                                                                      mdns_cache_record_mask_t record_mask);

/**
 * @brief Notify the resolver from the service cache.
 *
 * @param entry Pointer to the cache entry.
 * @param service Pointer to the service cache.
 * @param resolver Pointer to the resolver.
 * @return true if successfully notified, false otherwise.
 */
bool mdns_priv_resolver_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                  mdns_resolver_t *resolver);

/**
 * @brief Notify the resolver from the service cache for a goodbye message.
 *
 * @param entry Pointer to the cache entry.
 * @param service Pointer to the service cache.
 * @param record_mask Bitmask of records to update.
 * @param subtype Subtype name. NULL if no subtype is required.
 * @return true if successfully notified, false otherwise.
 */
bool mdns_priv_resolver_notify_goodbye_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                          mdns_cache_record_mask_t record_mask, const char *subtype);
#ifdef __cplusplus
}
#endif
