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
 * @brief Update a PTR record for service cache `_service._proto`.
 *
 * @param esp_netif     Pointer to the esp_netif.
 * @param ip_protocol   IP protocol.
 * @param instance      The instance name.
 * @param service       The service name.
 * @param proto         The protocol.
 * @param ttl           The PTR TTL.
 *
 * @return The result of the update, see @ref mdns_cache_update_result_t.
 *
 * @note The PTR record will be marked to-sync when:
 *      - A new service is added to the cache.
 *      - The PTR TTL is updated.
 *      - Receives a TTL=0 goodbye: all subscribing browsers will be notified of the goodbye.
 *        Then the whole service cache entry will be removed.
 */
mdns_cache_update_result_t mdns_priv_cache_update_ptr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *instance, const char *service, const char *proto,
                                                      uint32_t ttl);

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
 *         If the cache entry does not exist, return MDNS_CACHE_NO_CHANGE.
 *
 * @note When an A/AAAA record is updated (added, removed, or updated),
 *       all services under this cache entry will be marked to-sync for browses.
 *
 * @note Used by browses in case ADDR records come before SRV record.
 */
mdns_cache_update_result_t mdns_priv_cache_update_existing_addr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                                const char *hostname, const esp_ip_addr_t *addr, uint32_t ttl);

/**
 * @brief Clear all cache entries.
 */
void mdns_priv_cache_clear(void);

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

#ifdef __cplusplus
}
#endif
