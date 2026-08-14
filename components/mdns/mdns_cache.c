/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
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

static inline bool names_equal(const char *a, const char *b)
{
    return !mdns_utils_str_null_or_empty(a) && !mdns_utils_str_null_or_empty(b) && strcasecmp(a, b) == 0;
}

static inline bool nullable_names_equal(const char *a, const char *b)
{
    return (mdns_utils_str_null_or_empty(a) && mdns_utils_str_null_or_empty(b)) || names_equal(a, b);
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

            if (!old_entry->service_cache_list) {
                cache_remove_entry(old_entry);
            }
            return true;
        }
        old_entry_cache = &(*old_entry_cache)->next;
    }

    ESP_LOGE(TAG, "No target service in old entry");
    return false;
}

static bool cache_remove_service(mdns_cache_entry_t *entry, mdns_service_cache_t *service_entry)
{
    if (!entry || !service_entry) {
        return false;
    }

    mdns_service_cache_t **service_entry_ptr = &entry->service_cache_list;
    while (*service_entry_ptr) {
        if (*service_entry_ptr == service_entry) {
            *service_entry_ptr = service_entry->next;
            service_entry_free(service_entry);
            cache_remove_entry_if_empty(entry);
            return true;
        }
        service_entry_ptr = &(*service_entry_ptr)->next;
    }

    return false;
}
