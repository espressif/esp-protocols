/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "esp_check.h"
#include "esp_log.h"
#include "mdns_browser.h"
#include "mdns_cache.h"
#include "mdns_mem_caps.h"
#include "mdns_querier.h"
#include "mdns_utils.h"

static const char *TAG = "mdns_cache";

static mdns_cache_entry_t *s_cache;
static mdns_cache_expiry_t *s_expiry;
static bool s_sync_pending;

static inline bool names_equal(const char *a, const char *b)
{
    return !mdns_utils_str_null_or_empty(a) && !mdns_utils_str_null_or_empty(b) && strcasecmp(a, b) == 0;
}

static inline bool nullable_names_equal(const char *a, const char *b)
{
    return (mdns_utils_str_null_or_empty(a) && mdns_utils_str_null_or_empty(b)) || names_equal(a, b);
}

static inline int64_t calc_expiration_time_us(uint32_t ttl)
{
    return esp_timer_get_time() + (int64_t)ttl * MDNS_US_PER_SEC;
}

/**
 * @brief Insert a new expiry node to the head of `s_expiry` list.
 *
 * @note Should call `expiry_schedule` after this function to schedule the node to the correct position in the list.
 */
static bool expiry_new(mdns_cache_record_type_t type, mdns_cache_entry_t *entry,
                       mdns_service_cache_t *service, mdns_cache_addr_t *addr)
{
    mdns_cache_expiry_t *node = mdns_mem_calloc(1, sizeof(mdns_cache_expiry_t));
    if (!node) {
        HOOK_MALLOC_FAILED;
        return false;
    }

    node->entry = entry;
    node->service = service;
    node->addr = addr;
    node->record_mask = (mdns_cache_record_mask_t)type;
    node->queued = false;
    node->expires_at_us = INT64_MAX;
    node->next = s_expiry;
    s_expiry = node;
    return true;
}

static void expiry_schedule(mdns_cache_record_mask_t record_mask, mdns_cache_entry_t *entry,
                            mdns_service_cache_t *service, mdns_cache_addr_t *addr)
{
    if (!entry || (record_mask == MDNS_CACHE_RECORD_ADDR ? !addr : !service)) {
        return;
    }

    mdns_cache_expiry_t **link = &s_expiry;
    mdns_cache_expiry_t *node = NULL;
    uint32_t ttl = 0;

    while (*link) {
        mdns_cache_expiry_t *it = *link;
        if (it->record_mask == record_mask && it->entry == entry && (record_mask == MDNS_CACHE_RECORD_ADDR ?
                                                                     it->addr == addr : it->service == service)) {
            node = it;
            *link = it->next;
            break;
        }
        link = &it->next;
    }

    if (!node) {
        ESP_LOGE(TAG, "Cannot find expiry node for cache record");
        return;
    }

    switch (record_mask) {
    case MDNS_CACHE_RECORD_PTR:
        ttl = service->ptr_ttl;
        break;
    case MDNS_CACHE_RECORD_SRV:
        ttl = service->srv_ttl;
        break;
    case MDNS_CACHE_RECORD_TXT:
        ttl = service->txt_ttl;
        break;
    case MDNS_CACHE_RECORD_ADDR:
        ttl = addr->ttl;
        break;
    default:
        mdns_mem_free(node);
        return;
    }

    // If expiration time is extended, we can start the traverse from current node; otherwise start from the beginning.
    const int64_t new_expiration_time_us = ttl == 0 ? 0 : calc_expiration_time_us(ttl);
    const bool extended = new_expiration_time_us > node->expires_at_us && node->queued;
    node->expires_at_us = new_expiration_time_us;
    if (!extended) {
        link = &s_expiry;
    }

    while (*link && (*link)->expires_at_us < new_expiration_time_us) {
        link = &(*link)->next;
    }
    node->next = *link;
    *link = node;
    node->queued = true;
}

static bool update_ttl(uint32_t *cached_ttl, uint32_t ttl)
{
    if (*cached_ttl == ttl) {
        return false;
    }
    *cached_ttl = ttl;
    return true;
}

static bool service_match(const mdns_service_cache_t *cache, const char *instance, const char *service, const char *proto)
{
    return names_equal(cache->instance_name, instance) && names_equal(cache->service, service)
           && names_equal(cache->proto, proto);
}

static bool addr_equal(const esp_ip_addr_t *a, const esp_ip_addr_t *b)
{
    if (a->type != b->type) {
        return false;
    }

#ifdef CONFIG_LWIP_IPV6
    if (a->type == ESP_IPADDR_TYPE_V6) {
        return !memcmp(a->u_addr.ip6.addr, b->u_addr.ip6.addr, sizeof(a->u_addr.ip6.addr));
    }
#endif
#ifdef CONFIG_LWIP_IPV4
    if (a->type == ESP_IPADDR_TYPE_V4) {
        return a->u_addr.ip4.addr == b->u_addr.ip4.addr;
    }
#endif

    return false;
}

/**
 * @note Ensure service is not NULL before calling this function.
 */
static bool service_cache_is_empty(const mdns_service_cache_t *service)
{
    return service && (!service->ptr_present && !service->srv_present && !service->txt_present);
}

/**
 * @brief Mark sync out flags for a service cache.
 *
 * @param service_entry Service cache entry to mark sync out flags for.
 * @param result Result of the update operation.
 * @param records Record types to mark sync out flags for.
 * @param consumer Consumer type to mark sync out flags for.
 */
static void service_cache_mark_sync_out(mdns_service_cache_t *service_entry, mdns_cache_update_result_t result,
                                        mdns_cache_record_mask_t records, mdns_cache_consumer_mask_t consumer)
{
    if (service_entry && (result == MDNS_CACHE_ADDED || result == MDNS_CACHE_UPDATED)) {
        service_entry->sync_records |= records;
        service_entry->sync_consumers |= consumer;
        s_sync_pending = true;
    }
}

/**
 * @brief Clear sync out flags for a service cache.
 *
 * @note For browsers, only clear MDNS_CACHE_CONSUMER_BROWSE from sync_consumers.
 *
 * @param service_entry Service cache entry to clear sync out flags for.
 * @param consumer Consumer type to clear sync out flags for.
 */
static void service_cache_clear_sync_out(mdns_service_cache_t *service_entry, mdns_cache_consumer_type_t consumer)
{
    if (!service_entry) {
        return;
    }

    service_entry->sync_consumers &= ~(mdns_cache_consumer_mask_t)consumer;

    if (service_entry->sync_consumers == 0) {
        service_entry->sync_records = 0;
    }
}

/**
 * @brief Find a cache entry by hostname, esp_netif, and ip_protocol.
 *
 * @param hostname      The hostname to find.
 * @param esp_netif     Pointer to the esp_netif to find.
 * @param ip_protocol   IP protocol to find.
 *
 * @return Pointer to the cache entry if found, NULL otherwise.
 */
static mdns_cache_entry_t *cache_find_entry(const char *hostname, const esp_netif_t *esp_netif,
                                            mdns_ip_protocol_t ip_protocol)
{
    mdns_cache_entry_t *entry = s_cache;
    while (entry) {
        if (nullable_names_equal(entry->hostname, hostname) && entry->esp_netif == esp_netif
                && entry->ip_protocol == ip_protocol) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief Find a service cache entry by esp_netif, ip_protocol, instance, service, and proto.
 *
 * @param esp_netif     Pointer to the esp_netif to find.
 * @param ip_protocol   IP protocol to find.
 * @param instance      The instance name to find.
 * @param service       The service name to find.
 * @param proto         The protocol to find.
 * @param owner_entry   Receives the pointer to the cache entry that owns the service cache.
 *
 * @return Pointer to the service cache entry if found, NULL otherwise.
 */
static mdns_service_cache_t *cache_find_service(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                const char *instance, const char *service, const char *proto,
                                                mdns_cache_entry_t **owner_entry)
{
    mdns_cache_entry_t *entry = s_cache;
    if (owner_entry) {
        *owner_entry = NULL;
    }

    while (entry) {
        if (entry->esp_netif == esp_netif && entry->ip_protocol == ip_protocol) {
            mdns_service_cache_t *service_entry = entry->service_cache_list;
            while (service_entry) {
                if (service_match(service_entry, instance, service, proto)) {
                    if (owner_entry) {
                        *owner_entry = entry;
                    }
                    return service_entry;
                }
                service_entry = service_entry->next;
            }
        }
        entry = entry->next;
    }
    return NULL;
}

bool mdns_priv_cache_host_has_service(const char *hostname, const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                      const char *service, const char *proto)
{
    mdns_cache_entry_t *entry = cache_find_entry(hostname, esp_netif, ip_protocol);
    if (!entry) {
        return false;
    }

    for (const mdns_service_cache_t *it = entry->service_cache_list; it; it = it->next) {
        if (names_equal(it->service, service) && names_equal(it->proto, proto)) {
            return true;
        }
    }

    return false;
}

static void service_entry_free(mdns_service_cache_t *service_entry)
{
    mdns_mem_free(service_entry->instance_name);
    mdns_mem_free(service_entry->service);
    mdns_mem_free(service_entry->proto);
    while (service_entry->txt_list) {
        mdns_txt_linked_item_t *txt = service_entry->txt_list;
        service_entry->txt_list = service_entry->txt_list->next;
        mdns_mem_free((char *)txt->key);
        mdns_mem_free(txt->value);
        mdns_mem_free(txt);
    }
    mdns_mem_free(service_entry);
}

static void cache_entry_free(mdns_cache_entry_t *entry)
{
    mdns_mem_free(entry->hostname);
    while (entry->addr_list) {
        mdns_cache_addr_t *addr = entry->addr_list;
        entry->addr_list = entry->addr_list->next;
        mdns_mem_free(addr);
    }
    while (entry->service_cache_list) {
        mdns_service_cache_t *service_entry = entry->service_cache_list;
        entry->service_cache_list = entry->service_cache_list->next;
        service_entry_free(service_entry);
    }
    mdns_mem_free(entry);
}

static bool cache_remove_entry(mdns_cache_entry_t *entry)
{
    if (!entry) {
        return false;
    }

    mdns_cache_entry_t **entry_ptr = &s_cache;
    while (*entry_ptr) {
        if (*entry_ptr == entry) {
            *entry_ptr = entry->next;
            cache_entry_free(entry);
            return true;
        }
        entry_ptr = &(*entry_ptr)->next;
    }

    return false;
}

static bool cache_remove_entry_if_empty(mdns_cache_entry_t *entry)
{
    if (!entry || entry->addr_list || entry->service_cache_list) {
        return false;
    }
    return cache_remove_entry(entry);
}

static mdns_cache_entry_t *cache_add_entry(const char *hostname, const esp_netif_t *esp_netif,
                                           mdns_ip_protocol_t ip_protocol)
{
    mdns_cache_entry_t *entry = mdns_mem_calloc(1, sizeof(mdns_cache_entry_t));
    if (!entry) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    if (!mdns_utils_str_null_or_empty(hostname)) {
        entry->hostname = mdns_mem_strdup(hostname);
        if (!entry->hostname) {
            HOOK_MALLOC_FAILED;
            mdns_mem_free(entry);
            return NULL;
        }
    }

    entry->esp_netif = (esp_netif_t *)esp_netif;
    entry->ip_protocol = ip_protocol;
    entry->next = s_cache;
    s_cache = entry;
    return entry;
}

static mdns_cache_entry_t *cache_get_or_add_entry(const char *hostname, const esp_netif_t *esp_netif,
                                                  mdns_ip_protocol_t ip_protocol)
{
    mdns_cache_entry_t *entry = cache_find_entry(hostname, esp_netif, ip_protocol);
    return entry ? entry : cache_add_entry(hostname, esp_netif, ip_protocol);
}

static mdns_service_cache_t *cache_add_service(mdns_cache_entry_t *entry, const char *instance, const char *service,
                                               const char *proto)
{
    if (!entry) {
        return NULL;
    }

    mdns_service_cache_t *service_entry = mdns_mem_calloc(1, sizeof(mdns_service_cache_t));
    if (!service_entry) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    if (!mdns_utils_str_null_or_empty(instance)) {
        service_entry->instance_name = mdns_mem_strdup(instance);
        if (!service_entry->instance_name) {
            HOOK_MALLOC_FAILED;
            service_entry_free(service_entry);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(service)) {
        service_entry->service = mdns_mem_strdup(service);
        if (!service_entry->service) {
            HOOK_MALLOC_FAILED;
            service_entry_free(service_entry);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(proto)) {
        service_entry->proto = mdns_mem_strdup(proto);
        if (!service_entry->proto) {
            HOOK_MALLOC_FAILED;
            service_entry_free(service_entry);
            return NULL;
        }
    }

    service_entry->next = entry->service_cache_list;
    entry->service_cache_list = service_entry;
    return service_entry;
}

/**
 * @brief Move a service cache from one cache entry to another.
 *
 * @param old_entry The old cache entry to move the service from.
 * @param new_entry The new cache entry to move the service to.
 * @param cache The service cache to move.
 *
 * @return true if the service cache was moved successfully, false otherwise.
 *
 * @note This function only moves the service cache and will not create or delete old entries.
 */
static bool cache_move_service(mdns_cache_entry_t *old_entry, mdns_cache_entry_t *new_entry, mdns_service_cache_t *cache)
{
    if (!old_entry || !new_entry || !cache) {
        return false;
    }

    mdns_service_cache_t **old_entry_cache = &old_entry->service_cache_list;

    while (*old_entry_cache) {
        if (*old_entry_cache == cache) {
            *old_entry_cache = cache->next;
            cache->next = new_entry->service_cache_list;
            new_entry->service_cache_list = cache;

            // Update the entry pointer for all expiration nodes of the service cache.
            for (mdns_cache_expiry_t *it = s_expiry; it; it = it->next) {
                if (it->service == cache) {
                    it->entry = new_entry;
                }
            }
            return true;
        }
        old_entry_cache = &(*old_entry_cache)->next;
    }

    ESP_LOGE(TAG, "No target service in old entry");
    return false;
}

static void cache_remove_service_if_empty(mdns_cache_entry_t *entry, mdns_service_cache_t *service_entry)
{
    if (!entry || !service_cache_is_empty(service_entry)) {
        return;
    }

    mdns_service_cache_t **service_entry_ptr = &entry->service_cache_list;
    while (*service_entry_ptr) {
        if (*service_entry_ptr == service_entry) {
            *service_entry_ptr = service_entry->next;
            service_entry_free(service_entry);
            cache_remove_entry_if_empty(entry);
            return;
        }
        service_entry_ptr = &(*service_entry_ptr)->next;
    }
}

mdns_cache_update_result_t mdns_priv_cache_update_ptr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *instance, const char *service, const char *proto,
                                                      uint32_t ttl)
{
    if (mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service)
            || mdns_utils_str_null_or_empty(proto)) {
        return MDNS_CACHE_NO_CHANGE;
    }

    mdns_cache_entry_t *owner_entry = NULL;
    mdns_service_cache_t *service_entry = cache_find_service(esp_netif, ip_protocol, instance,
                                                             service, proto, &owner_entry);
    mdns_cache_update_result_t result = MDNS_CACHE_NO_CHANGE;
    bool new_service = false;

    if (ttl == 0) {
        if (!service_entry || !service_entry->ptr_present) {
            return MDNS_CACHE_NO_CHANGE;
        }

        service_entry->ptr_ttl = 0;
        expiry_schedule(MDNS_CACHE_RECORD_PTR, owner_entry, service_entry, NULL);
        return MDNS_CACHE_UPDATED;
    }

    if (!service_entry) {
        // Uncached PTR records will all be stored in hostname == NULL entry temporarily
        owner_entry = cache_get_or_add_entry(NULL, esp_netif, ip_protocol);
        if (!owner_entry) {
            return MDNS_CACHE_ERROR;
        }

        service_entry = cache_add_service(owner_entry, instance, service, proto);
        if (!service_entry) {
            cache_remove_entry_if_empty(owner_entry);
            return MDNS_CACHE_ERROR;
        }
        new_service = true;
    }

    if (!service_entry->ptr_present) {
        if (!expiry_new(MDNS_CACHE_RECORD_PTR, owner_entry, service_entry, NULL)) {
            ESP_LOGE(TAG, "Failed to create PTR expiry");
            cache_remove_service_if_empty(owner_entry, service_entry);
            return MDNS_CACHE_ERROR;
        }
    }

    bool changed = !service_entry->ptr_present;
    service_entry->ptr_present = true;
    changed |= update_ttl(&service_entry->ptr_ttl, ttl);
    expiry_schedule(MDNS_CACHE_RECORD_PTR, owner_entry, service_entry, NULL);

    if (changed) {
        result = MDNS_CACHE_UPDATED;
    }

    if (new_service) {
        result = MDNS_CACHE_ADDED;
    }

    service_cache_mark_sync_out(service_entry, result, MDNS_CACHE_RECORD_PTR, MDNS_CACHE_CONSUMER_BROWSE);
    return result;
}

static mdns_cache_update_result_t service_cache_srv_update(mdns_service_cache_t *cache, uint16_t priority, uint16_t weight,
                                                           uint16_t port, uint32_t ttl)
{
    mdns_cache_update_result_t result = MDNS_CACHE_NO_CHANGE;

    if (!cache->srv_present) {
        cache->srv_present = true;
        result = MDNS_CACHE_UPDATED;
    }
    if (cache->priority != priority) {
        cache->priority = priority;
        result = MDNS_CACHE_UPDATED;
    }
    if (cache->weight != weight) {
        cache->weight = weight;
        result = MDNS_CACHE_UPDATED;
    }
    if (cache->port != port) {
        cache->port = port;
        result = MDNS_CACHE_UPDATED;
    }
    if (update_ttl(&cache->srv_ttl, ttl)) {
        result = MDNS_CACHE_UPDATED;
    }

    return result;
}

mdns_cache_update_result_t mdns_priv_cache_update_srv(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *hostname, const char *instance, const char *service,
                                                      const char *proto, uint16_t priority, uint16_t weight,
                                                      uint16_t port, uint32_t ttl)
{
    mdns_cache_entry_t *owner_entry = NULL;
    mdns_service_cache_t *service_entry = cache_find_service(esp_netif, ip_protocol, instance, service,
                                                             proto, &owner_entry);
    mdns_cache_entry_t *host_entry = NULL;
    mdns_cache_update_result_t result = MDNS_CACHE_NO_CHANGE;

    if (ttl == 0) {
        if (!service_entry || !service_entry->srv_present) {
            return MDNS_CACHE_NO_CHANGE;
        }

        service_entry->srv_ttl = 0;
        expiry_schedule(MDNS_CACHE_RECORD_SRV, owner_entry, service_entry, NULL);
        return MDNS_CACHE_UPDATED;
    }

    if (owner_entry && names_equal(owner_entry->hostname, hostname)) {
        host_entry = owner_entry;
    } else {
        host_entry = cache_get_or_add_entry(hostname, esp_netif, ip_protocol);
        if (!host_entry) {
            return MDNS_CACHE_ERROR;
        }
    }

    if (!service_entry) {
        service_entry = cache_add_service(host_entry, instance, service, proto);
        if (!service_entry) {
            cache_remove_entry_if_empty(host_entry);
            return MDNS_CACHE_ERROR;
        }
        result = MDNS_CACHE_ADDED;
    } else if (owner_entry != host_entry) {
        if (!cache_move_service(owner_entry, host_entry, service_entry)) {
            cache_remove_entry_if_empty(host_entry);
            return MDNS_CACHE_ERROR;
        }
        result = MDNS_CACHE_UPDATED;
    }

    if (!service_entry->srv_present) {
        // If the expiry creation failed, move the service cache back if host_entry is not NULL, otherwise remove the service cache.
        if (!expiry_new(MDNS_CACHE_RECORD_SRV, host_entry, service_entry, NULL)) {
            ESP_LOGE(TAG, "Failed to create SRV expiry");
            if (owner_entry) {
                (void)cache_move_service(host_entry, owner_entry, service_entry);
                cache_remove_entry_if_empty(host_entry);
            } else {
                cache_remove_service_if_empty(host_entry, service_entry);
            }
            return MDNS_CACHE_ERROR;
        }
    }
    cache_remove_entry_if_empty(owner_entry);

    mdns_cache_update_result_t srv_result = service_cache_srv_update(service_entry, priority, weight, port, ttl);
    expiry_schedule(MDNS_CACHE_RECORD_SRV, host_entry, service_entry, NULL);
    if (result == MDNS_CACHE_NO_CHANGE) {
        result = srv_result;
    }

    service_cache_mark_sync_out(service_entry, result, MDNS_CACHE_RECORD_SRV, MDNS_CACHE_CONSUMER_BROWSE);
    return result;
}

static bool txt_item_equal(const mdns_txt_linked_item_t *a, const mdns_txt_linked_item_t *b)
{
    if (!a || !b || !names_equal(a->key, b->key)) {
        return false;
    }
    if (a->value_len != b->value_len) {
        return false;
    }
    if (!a->value && !b->value) {
        return true;
    }
    return a->value && b->value && memcmp(a->value, b->value, a->value_len) == 0;
}

static bool txt_list_contains(const mdns_txt_linked_item_t *txt_list, const mdns_txt_linked_item_t *item)
{
    while (txt_list) {
        if (txt_item_equal(txt_list, item)) {
            return true;
        }
        txt_list = txt_list->next;
    }
    return false;
}

static size_t txt_list_count(const mdns_txt_linked_item_t *txt_list)
{
    size_t count = 0;
    while (txt_list) {
        count++;
        txt_list = txt_list->next;
    }
    return count;
}

static bool txt_list_equal(const mdns_txt_linked_item_t *a, const mdns_txt_linked_item_t *b)
{
    if (a == b) {
        return true;
    }

    if (txt_list_count(a) != txt_list_count(b)) {
        return false;
    }

    for (const mdns_txt_linked_item_t *it = a; it; it = it->next) {
        if (!txt_list_contains(b, it)) {
            return false;
        }
    }

    return true;
}

static mdns_cache_update_result_t service_cache_txt_update(mdns_service_cache_t *service_entry,
                                                           mdns_txt_linked_item_t *new_txt, uint32_t ttl)
{
    if (!service_entry) {
        mdns_utils_free_txt_linked_list(new_txt);
        return MDNS_CACHE_ERROR;
    }

    if (txt_list_equal(service_entry->txt_list, new_txt) && service_entry->txt_present
            && !update_ttl(&service_entry->txt_ttl, ttl)) {
        mdns_utils_free_txt_linked_list(new_txt);
        return MDNS_CACHE_NO_CHANGE;
    }

    service_entry->txt_present = true;
    service_entry->txt_ttl = ttl;
    mdns_txt_linked_item_t *old_txt = service_entry->txt_list;
    service_entry->txt_list = new_txt;
    mdns_utils_free_txt_linked_list(old_txt);
    return MDNS_CACHE_UPDATED;
}

mdns_cache_update_result_t mdns_priv_cache_update_txt(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                      const char *instance, const char *service, const char *proto,
                                                      mdns_txt_linked_item_t *txt, uint32_t ttl)
{
    mdns_cache_entry_t *owner_entry = NULL;
    mdns_service_cache_t *service_entry = cache_find_service(esp_netif, ip_protocol, instance,
                                                             service, proto, &owner_entry);
    mdns_cache_update_result_t result = MDNS_CACHE_NO_CHANGE;
    bool new_service = false;

    if (ttl == 0) {
        mdns_utils_free_txt_linked_list(txt);
        if (!service_entry || !service_entry->txt_present) {
            return MDNS_CACHE_NO_CHANGE;
        }

        service_entry->txt_ttl = 0;
        expiry_schedule(MDNS_CACHE_RECORD_TXT, owner_entry, service_entry, NULL);
        return MDNS_CACHE_UPDATED;
    }

    if (!service_entry) {
        owner_entry = cache_get_or_add_entry(NULL, esp_netif, ip_protocol);
        if (!owner_entry) {
            mdns_utils_free_txt_linked_list(txt);
            return MDNS_CACHE_ERROR;
        }

        service_entry = cache_add_service(owner_entry, instance, service, proto);
        if (!service_entry) {
            cache_remove_entry_if_empty(owner_entry);
            mdns_utils_free_txt_linked_list(txt);
            return MDNS_CACHE_ERROR;
        }

        new_service = true;
    }

    if (!service_entry->txt_present) {
        if (!expiry_new(MDNS_CACHE_RECORD_TXT, owner_entry, service_entry, NULL)) {
            ESP_LOGE(TAG, "Failed to create TXT expiry");
            cache_remove_service_if_empty(owner_entry, service_entry);
            mdns_utils_free_txt_linked_list(txt);
            return MDNS_CACHE_ERROR;
        }
    }

    result = service_cache_txt_update(service_entry, txt, ttl);
    expiry_schedule(MDNS_CACHE_RECORD_TXT, owner_entry, service_entry, NULL);
    if (new_service) {
        result = MDNS_CACHE_ADDED;
    }

    service_cache_mark_sync_out(service_entry, result, MDNS_CACHE_RECORD_TXT, MDNS_CACHE_CONSUMER_BROWSE);
    return result;
}

static mdns_cache_update_result_t cache_update_addr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                    const char *hostname, const esp_ip_addr_t *addr, uint32_t ttl,
                                                    bool create_if_absent)
{
    mdns_cache_entry_t *entry = NULL;
    bool addr_added = false;
    bool ttl_changed = false;
    mdns_cache_update_result_t result = MDNS_CACHE_NO_CHANGE;

    if (ttl == 0) {
        entry = cache_find_entry(hostname, esp_netif, ip_protocol);
        if (!entry) {
            return MDNS_CACHE_NO_CHANGE;
        }

        // Remove addr
        for (mdns_cache_addr_t *it = entry->addr_list; it; it = it->next) {
            if (addr_equal(&it->addr, addr)) {
                it->ttl = 0;
                expiry_schedule(MDNS_CACHE_RECORD_ADDR, entry, NULL, it);
                return MDNS_CACHE_UPDATED;
            }
        }

        return MDNS_CACHE_NO_CHANGE;
    }

    entry = create_if_absent ? cache_get_or_add_entry(hostname, esp_netif, ip_protocol)
            : cache_find_entry(hostname, esp_netif, ip_protocol);
    if (!entry) {
        return create_if_absent ? MDNS_CACHE_ERROR : MDNS_CACHE_NO_CHANGE;
    }

    mdns_cache_addr_t **addr_link = &entry->addr_list;
    mdns_cache_addr_t *addr_entry = NULL;
    while (*addr_link) {
        if (addr_equal(&(*addr_link)->addr, addr)) {
            addr_entry = *addr_link;
            ttl_changed = update_ttl(&(*addr_link)->ttl, ttl);
            expiry_schedule(MDNS_CACHE_RECORD_ADDR, entry, NULL, addr_entry);
            break;
        }
        addr_link = &(*addr_link)->next;
    }

    if (!addr_entry) {
        mdns_cache_addr_t *new_addr = mdns_mem_calloc(1, sizeof(mdns_cache_addr_t));
        if (!new_addr) {
            HOOK_MALLOC_FAILED;
            cache_remove_entry_if_empty(entry);
            return MDNS_CACHE_ERROR;
        }

        if (!expiry_new(MDNS_CACHE_RECORD_ADDR, entry, NULL, new_addr)) {
            ESP_LOGE(TAG, "Failed to create ADDR expiry");
            mdns_mem_free(new_addr);
            cache_remove_entry_if_empty(entry);
            return MDNS_CACHE_ERROR;
        }
        new_addr->addr = *addr;
        new_addr->ttl = ttl;
        expiry_schedule(MDNS_CACHE_RECORD_ADDR, entry, NULL, new_addr);
        new_addr->next = entry->addr_list;
        entry->addr_list = new_addr;
        addr_added = true;
    }

    result = (addr_added || ttl_changed) ? MDNS_CACHE_UPDATED : MDNS_CACHE_NO_CHANGE;
    for (mdns_service_cache_t *service = entry->service_cache_list; service; service = service->next) {
        service_cache_mark_sync_out(service, result, MDNS_CACHE_RECORD_ADDR, MDNS_CACHE_CONSUMER_BROWSE);
    }

    return result;
}

mdns_cache_update_result_t mdns_priv_cache_update_addr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                       const char *hostname, const esp_ip_addr_t *addr, uint32_t ttl)
{
    return cache_update_addr(esp_netif, ip_protocol, hostname, addr, ttl, true);
}

mdns_cache_update_result_t mdns_priv_cache_update_existing_addr(const esp_netif_t *esp_netif, mdns_ip_protocol_t ip_protocol,
                                                                const char *hostname, const esp_ip_addr_t *addr, uint32_t ttl)
{
    mdns_cache_entry_t *entry = cache_find_entry(hostname, esp_netif, ip_protocol);
    if (!entry) {
        return MDNS_CACHE_NO_CHANGE;
    }

    // Only update ADDR records if consumers exist
    for (const mdns_service_cache_t *service = entry->service_cache_list; service; service = service->next) {
        if (mdns_priv_browse_has_service(service->service, service->proto)) {
            return cache_update_addr(esp_netif, ip_protocol, hostname, addr, ttl, false);
        }
    }
    return MDNS_CACHE_NO_CHANGE;
}

void mdns_priv_cache_remove_expired_records(int64_t now_us)
{
    if (!s_expiry || s_expiry->expires_at_us > now_us) {
        return;
    }

    while (s_expiry && s_expiry->expires_at_us <= now_us) {
        mdns_cache_expiry_t *node = s_expiry;
        mdns_cache_addr_t *addr = node->addr;
        mdns_cache_entry_t *entry = node->entry;
        mdns_service_cache_t *service = node->service;
        mdns_cache_record_mask_t record = node->record_mask;
        s_expiry = s_expiry->next;

        switch (record) {
        case MDNS_CACHE_RECORD_PTR:
            if (!service) {
                mdns_mem_free(node);
                continue;
            }
            service->ptr_present = false;
            service->ptr_ttl = 0;
            if (!mdns_priv_browse_notify_ptr_goodbye_from_service_cache(entry, service)) {
                ESP_LOGE(TAG, "Failed to notify PTR expiration");
            }
            break;
        case MDNS_CACHE_RECORD_SRV:
            if (!service) {
                mdns_mem_free(node);
                continue;
            }
            service->srv_present = false;
            service->priority = 0;
            service->weight = 0;
            service->port = 0;
            service->srv_ttl = 0;
            break;
        case MDNS_CACHE_RECORD_TXT:
            if (!service) {
                mdns_mem_free(node);
                continue;
            }
            service->txt_present = false;
            mdns_utils_free_txt_linked_list(service->txt_list);
            service->txt_list = NULL;
            service->txt_ttl = 0;
            break;
        case MDNS_CACHE_RECORD_ADDR:
            if (!addr) {
                mdns_mem_free(node);
                continue;
            }
            queueDetach(mdns_cache_addr_t, entry->addr_list, addr);
            mdns_mem_free(addr);

            for (mdns_service_cache_t *it = entry->service_cache_list; it; it = it->next) {
                service_cache_mark_sync_out(it, MDNS_CACHE_UPDATED, MDNS_CACHE_RECORD_ADDR, MDNS_CACHE_CONSUMER_BROWSE);
            }
            cache_remove_entry_if_empty(entry);
            break;
        }

        if (service && service_cache_is_empty(service)) {
            cache_remove_service_if_empty(entry, service);
        } else {
            service_cache_mark_sync_out(service, MDNS_CACHE_UPDATED, record, MDNS_CACHE_CONSUMER_BROWSE);
        }
        mdns_mem_free(node);
    }
}

void mdns_priv_cache_clear(void)
{
    while (s_cache) {
        mdns_cache_entry_t *entry = s_cache;
        s_cache = s_cache->next;
        cache_entry_free(entry);
    }
    while (s_expiry) {
        mdns_cache_expiry_t *node = s_expiry;
        s_expiry = s_expiry->next;
        mdns_mem_free(node);
    }
    s_sync_pending = false;
}

static bool project_txt(const mdns_txt_linked_item_t *txt_list, mdns_txt_item_t **out_txt, uint8_t **out_value_len,
                        size_t *out_count)
{
    esp_err_t __attribute__((unused))ret = ESP_OK;
    *out_txt = NULL;
    *out_value_len = NULL;
    *out_count = 0;

    size_t count = 0;
    for (const mdns_txt_linked_item_t *txt = txt_list; txt; txt = txt->next) {
        count++;
    }
    if (count == 0) {
        return true;
    }

    mdns_txt_item_t *txt_items = mdns_mem_calloc(count, sizeof(mdns_txt_item_t));
    if (!txt_items) {
        HOOK_MALLOC_FAILED;
        return false;
    }
    uint8_t *value_len = mdns_mem_calloc(count, sizeof(uint8_t));
    if (!value_len) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(txt_items);
        return false;
    }

    size_t i = 0;
    for (const mdns_txt_linked_item_t *txt = txt_list; txt; txt = txt->next, i++) {
        txt_items[i].key = mdns_mem_strdup(txt->key);
        ESP_GOTO_ON_FALSE(txt_items[i].key, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate key");

        value_len[i] = txt->value_len;
        if (!txt->value) {
            ESP_GOTO_ON_FALSE(txt->value_len == 0, ESP_ERR_INVALID_ARG, cleanup, TAG, "Invalid value");
            continue;
        }

        txt_items[i].value = mdns_mem_calloc(txt->value_len + 1, sizeof(char));
        ESP_GOTO_ON_FALSE(txt_items[i].value, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate value");
        memcpy((char *)txt_items[i].value, txt->value, txt->value_len);
    }

    *out_value_len = value_len;
    *out_count = count;
    *out_txt = txt_items;
    return true;

error:
    HOOK_MALLOC_FAILED;
cleanup:
    for (size_t i = 0; i < count; i++) {
        mdns_mem_free((char *)txt_items[i].key);
        mdns_mem_free((char *)txt_items[i].value);
    }
    mdns_mem_free(value_len);
    mdns_mem_free(txt_items);
    return false;
}

static bool project_addr(const mdns_cache_addr_t *addr_list, mdns_ip_addr_t **out_addr_list)
{
    mdns_ip_addr_t *head = NULL;
    mdns_ip_addr_t **tail = &head;

    *out_addr_list = NULL;

    for (const mdns_cache_addr_t *addr = addr_list; addr; addr = addr->next) {
        mdns_ip_addr_t *new_addr = mdns_mem_calloc(1, sizeof(mdns_ip_addr_t));
        if (!new_addr) {
            HOOK_MALLOC_FAILED;
            while (head) {
                mdns_ip_addr_t *next = head->next;
                mdns_mem_free(head);
                head = next;
            }
            return false;
        }
        new_addr->addr = addr->addr;
        *tail = new_addr;
        tail = &new_addr->next;
    }

    *out_addr_list = head;
    return true;
}

esp_err_t mdns_priv_service_cache_to_result(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                            mdns_result_t **out_result)
{
    esp_err_t ret = ESP_OK;
    mdns_result_t *result = mdns_mem_calloc(1, sizeof(mdns_result_t));
    ESP_GOTO_ON_FALSE(result, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate result");

    result->esp_netif = entry->esp_netif;
    result->ttl = service->ptr_ttl;
    result->ip_protocol = entry->ip_protocol;

    if (!mdns_utils_str_null_or_empty(service->instance_name)) {
        result->instance_name = mdns_mem_strdup(service->instance_name);
        ESP_GOTO_ON_FALSE(result->instance_name, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate instance name");
    }
    if (!mdns_utils_str_null_or_empty(service->service)) {
        result->service_type = mdns_mem_strdup(service->service);
        ESP_GOTO_ON_FALSE(result->service_type, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate service type");
    }
    if (!mdns_utils_str_null_or_empty(service->proto)) {
        result->proto = mdns_mem_strdup(service->proto);
        ESP_GOTO_ON_FALSE(result->proto, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate protocol");
    }

    if (service->srv_present) {
        if (entry->hostname) {
            result->hostname = mdns_mem_strdup(entry->hostname);
            ESP_GOTO_ON_FALSE(result->hostname, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate hostname");
        }
        result->port = service->port;
        // Address list is bound to hostname, so when srv is removed, the address list should not be projected.
        ESP_GOTO_ON_FALSE(project_addr(entry->addr_list, &result->addr), ESP_ERR_NO_MEM, error, TAG,
                          "Failed to project address list");
    }

    if (service->txt_present) {
        ESP_GOTO_ON_FALSE(project_txt(service->txt_list, &result->txt, &result->txt_value_len, &result->txt_count),
                          ESP_ERR_NO_MEM, error, TAG, "Failed to project TXT");
    }

    *out_result = result;
    return ret;

error:
    HOOK_MALLOC_FAILED;
    mdns_priv_query_results_free(result);
    return ret;
}

void mdns_priv_cache_process_sync(void)
{
    if (!s_sync_pending) {
        return;
    }
    s_sync_pending = false;

    for (mdns_cache_entry_t *entry = s_cache; entry; entry = entry->next) {
        for (mdns_service_cache_t *service = entry->service_cache_list; service; service = service->next) {
            if (service->sync_consumers & MDNS_CACHE_CONSUMER_BROWSE) {
                mdns_cache_record_mask_t records = service->sync_records;
                if (!mdns_priv_browse_update_from_service_cache(entry, service, records)) {
                    ESP_LOGW(TAG, "Failed to notify browse, dropping sync mark");
                }
                service_cache_clear_sync_out(service, MDNS_CACHE_CONSUMER_BROWSE);
            }
            // TODO: When resolver is implemented, add resolver sync processing here.
        }
    }
}

bool mdns_priv_cache_notify_browse(mdns_browse_t *browse)
{
    if (!browse) {
        return false;
    }

    bool notified = true;

    for (mdns_cache_entry_t *entry = s_cache; entry; entry = entry->next) {
        for (mdns_service_cache_t *service = entry->service_cache_list; service; service = service->next) {
            notified &= mdns_priv_browse_notify_from_service_cache(entry, service, browse);
        }
    }

    return notified;
}
