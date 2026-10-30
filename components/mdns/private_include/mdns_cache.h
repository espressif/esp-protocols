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
 * @brief Check if a host has a service by hostname, esp_netif, ip_protocol, service, and proto.
 *
 * @param hostname      The hostname to check.
 * @param esp_netif     Pointer to the esp_netif to check.
 * @param ip_protocol   IP protocol to check.
 * @param service       The service name to check.
 * @param proto         The protocol to check.
 *
 * @return true if the host has the service, false otherwise.
 */
bool mdns_priv_cache_host_has_service(const char *hostname, const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                      const char *service, const char *proto);

/**
 * @brief Find the subtype node in a service cache by subtype name.
 *
 * @param service Pointer to the service cache.
 * @param subtype The subtype name to find.
 *
 * @return Pointer to the subtype node if found, NULL otherwise.
 */
const mdns_cache_subtype_t *mdns_priv_cache_service_find_subtype(const mdns_service_cache_t *service, const char *subtype);

/**
 * @brief Update a PTR record for service cache `_service._proto` with possible subtype.
 *
 * @param esp_netif     Pointer to the esp_netif.
 * @param ip_protocol   IP protocol.
 * @param instance      The instance name.
 * @param service       The service name.
 * @param proto         The protocol.
 * @param subtype       The subtype.
 * @param ttl           The PTR TTL.
 *
 * @return The result of the update, see @ref mdns_cache_update_result_t.
 *
 * @note The PTR record will be marked to-sync when:
 *      - A new service is added to the cache.
 *      - A new subtype is appended to the service cache.
 *      - The PTR TTL is updated.
 *
 * @note A TTL=0 goodbye notifies only the matching base/subtype browsers and
 *       removes only that PTR record. Other cached records are preserved.
 */
mdns_cache_update_result_t mdns_priv_cache_update_ptr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *instance, const char *service, const char *proto,
                                                      const char *subtype, uint32_t ttl);

/**
 * @brief Update a SRV record for service cache `instance._service._proto`.
 *
 * @param esp_netif     Pointer to the esp_netif.
 * @param ip_protocol   IP protocol.
 * @param hostname      The hostname to update.
 * @param instance      The instance name.
 * @param service       The service name.
 * @param proto         The protocol.
 * @param priority      The priority to update.
 * @param weight        The weight to update.
 * @param port          The port to update.
 * @param ttl           The SRV TTL.
 *
 * @return The result of the update, see @ref mdns_cache_update_result_t.
 *
 * @note The SRV record will be marked to-sync when:
 *      - A new SRV record is added to the service cache.
 *      - The service cache is moved to the host designated by @ref hostname.
 *      - The SRV record is updated.
 *      - Receives a TTL=0 goodbye: the SRV record will be marked as absent.
 */
mdns_cache_update_result_t mdns_priv_cache_update_srv(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *hostname, const char *instance, const char *service,
                                                      const char *proto, uint16_t priority, uint16_t weight,
                                                      uint16_t port, uint32_t ttl);

/**
 * @brief Update a TXT record for service cache `instance._service._proto`.
 *
 * @param esp_netif     Pointer to the esp_netif.
 * @param ip_protocol   IP protocol.
 * @param instance      The instance name.
 * @param service       The service name.
 * @param proto         The protocol.
 * @param txt           The TXT record to update.
 * @param ttl           The TXT TTL.
 *
 * @return The result of the update, see @ref mdns_cache_update_result_t.
 *
 * @note The TXT record will be marked to-sync when:
 *      - A new TXT record is added to the service cache.
 *      - The TXT record is updated.
 *      - Receives a TTL=0 goodbye: the TXT record will be marked as absent.
 */
mdns_cache_update_result_t mdns_priv_cache_update_txt(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *instance, const char *service, const char *proto,
                                                      mdns_txt_linked_item_t *txt, uint32_t ttl);

/**
 * @brief Update an A/AAAA record for a cache entry `hostname`.
 *
 * @param esp_netif     Pointer to the esp_netif.
 * @param ip_protocol   IP protocol.
 * @param hostname      The hostname to update.
 * @param addr          The address to update.
 * @param ttl           The TTL.
 *
 * @return The result of the update, see @ref mdns_cache_update_result_t.
 *
 * @note When an A/AAAA record is updated (added, removed, or updated),
 *       all services under this cache entry will be marked to-sync for browses.
 */
mdns_cache_update_result_t mdns_priv_cache_update_addr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                       const char *hostname, const esp_ip_addr_t *addr, uint32_t ttl);

/**
 * @brief Update an A/AAAA record for an existing cache entry `hostname`.
 *
 * @param esp_netif     Pointer to the esp_netif.
 * @param ip_protocol   IP protocol.
 * @param hostname      The hostname to update.
 * @param addr          The address to update.
 * @param ttl           The TTL.
 *
 * @return The result of the update, see @ref mdns_cache_update_result_t.
 *         If the cache entry does not exist, or no consumers subscribe to the hostname or service under the hostname,
 *         return MDNS_CACHE_NO_CHANGE.
 *
 * @note When an A/AAAA record is updated (added, removed, or updated),
 *       all services under this cache entry will be marked to-sync for browses.
 *
 * @note Used by browses and PTR resolvers in case ADDR records come before SRV record.
 */
mdns_cache_update_result_t mdns_priv_cache_update_existing_addr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                                const char *hostname, const esp_ip_addr_t *addr, uint32_t ttl);

/**
 * @brief Remove expired records from the entire cache.
 *
 * @param now_us The current time in microseconds from esp_timer_get_time().
 *
 * @note This function is called in mDNS service task while holding service lock.
 *       PTR expiration notifies matching browses and resolvers immediately.
 *       Other expirations notify corresponding resolvers immediately and mark to-sync for browses.
 */
void mdns_priv_cache_remove_expired_records(int64_t now_us);

/**
 * @brief Trim down the expiration time of unsubscribed records.
 *
 * @param now_us The current time in microseconds from esp_timer_get_time().
 *
 * @note This function scans through s_expiry and sets the expiration time of unsubscribed records to
 *       `min(expires_at_us, now_us + MDNS_CACHE_UNSUBSCRIBED_GRACE_SEC * MDNS_US_PER_SEC)`.
 *       Finally cleaned up in service task by `mdns_priv_cache_remove_expired_records()`.
 */
void mdns_priv_cache_trim_unsubscribed_records(int64_t now_us);

/**
 * @brief Clear all cache entries.
 */
void mdns_priv_cache_clear(void);

/**
 * @brief Deep copy a TXT linked list to arrays as required by `mdns_result_t` and `mdns_txt_resolver_result_t`.
 *
 * @param txt_list Pointer to the input TXT linked list.
 * @param out_txt Pointer to the output TXT array.
 * @param out_value_len Pointer to the output TXT value length array.
 * @param out_count Pointer to the output TXT count.
 * @return true on success, false on failure.
 *
 * @note If return value is true, the output arrays will be allocated and the caller is responsible for freeing them.
 *       Otherwise, `*out_txt, *out_value_len` will be set to NULL and `*out_count` will be set to 0.
 */
bool mdns_priv_cache_copy_txt(const mdns_txt_linked_item_t *txt_list, mdns_txt_item_t **out_txt, uint8_t **out_value_len,
                              size_t *out_count);

/**
 * @brief Convert a service cache to a mdns_result_t.
 *
 * @param entry       Pointer to the cache entry.
 * @param service     Pointer to the service cache.
 * @param out_result  Pointer to the output mdns_result_t.
 * @return ESP_OK on success, ESP_ERR_NO_MEM on allocation failure.
 */
esp_err_t mdns_priv_service_cache_to_result(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                            mdns_result_t **out_result);

/**
 * @brief Process all pending synchronization cache entries.
 *
 * @note This function is called by the MDNS task to process all pending synchronization cache entries.
 *       It will generate temporary results, notify corresponding consumers, and clear the sync out flags.
 */
void mdns_priv_cache_process_sync(void);

#ifdef CONFIG_MDNS_ENABLE_BROWSE
/**
 * @brief Replay all currently visible cache services to a newly registered browse.
 *
 * @return true if successfully notified, false otherwise
 */
bool mdns_priv_cache_notify_browse(mdns_browse_t *browse);
#endif

#ifdef CONFIG_MDNS_ENABLE_RESOLVER
/**
 * @brief Replay all currently visible cache services to a newly registered resolver.
 *
 * @param resolver Pointer to the resolver to notify.
 * @return true if successfully notified, false otherwise.
 */
bool mdns_priv_cache_notify_resolver(mdns_resolver_t *resolver);
#endif
#ifdef __cplusplus
}
#endif
