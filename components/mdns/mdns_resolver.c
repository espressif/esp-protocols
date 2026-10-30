/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_check.h"
#include "esp_log.h"
#include "mdns_cache.h"
#include "mdns_mem_caps.h"
#include "mdns_netif.h"
#include "mdns_querier.h"
#include "mdns_resolver.h"
#include "mdns_responder.h"
#include "mdns_service.h"
#include "mdns_utils.h"

static const char *TAG = "mdns_resolver";

static mdns_resolver_t *s_resolver;

static mdns_cache_record_type_t resolver_type_to_record_type(mdns_resolver_t *resolver)
{
    if (!resolver) {
        return 0;
    }

    switch (resolver->type) {
    case MDNS_RESOLVER_TYPE_PTR:
        return mdns_utils_str_null_or_empty(resolver->subtype) ? MDNS_CACHE_RECORD_PTR : MDNS_CACHE_RECORD_SUBTYPE;
    case MDNS_RESOLVER_TYPE_SRV:
        return MDNS_CACHE_RECORD_SRV;
    case MDNS_RESOLVER_TYPE_TXT:
        return MDNS_CACHE_RECORD_TXT;
    case MDNS_RESOLVER_TYPE_A:
        return MDNS_CACHE_RECORD_A;
    case MDNS_RESOLVER_TYPE_AAAA:
        return MDNS_CACHE_RECORD_AAAA;
    default:
        return 0;
    }
}

static bool resolvers_match(const mdns_resolver_t *a, const mdns_resolver_t *b)
{
    if (!a || !b || a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case MDNS_RESOLVER_TYPE_PTR:
        return mdns_utils_str_case_equal(a->service, b->service)
               && mdns_utils_str_case_equal(a->proto, b->proto)
               && mdns_utils_str_case_equal_nullable(a->subtype, b->subtype);
    case MDNS_RESOLVER_TYPE_SRV:
    case MDNS_RESOLVER_TYPE_TXT:
        return mdns_utils_str_case_equal(a->instance, b->instance)
               && mdns_utils_str_case_equal(a->service, b->service)
               && mdns_utils_str_case_equal(a->proto, b->proto);
    case MDNS_RESOLVER_TYPE_A:
    case MDNS_RESOLVER_TYPE_AAAA:
        return mdns_utils_str_case_equal(a->hostname, b->hostname);
    default:
        return false;
    }
}

static bool cache_addr_matches_resolver_type(const mdns_cache_addr_t *addr, mdns_resolver_type_t type)
{
    if (!addr || (type != MDNS_RESOLVER_TYPE_A && type != MDNS_RESOLVER_TYPE_AAAA)) {
        return false;
    }

#ifdef CONFIG_LWIP_IPV4
    if (type == MDNS_RESOLVER_TYPE_A && addr->addr.type == ESP_IPADDR_TYPE_V4) {
        return true;
    }
#endif
#ifdef CONFIG_LWIP_IPV6
    if (type == MDNS_RESOLVER_TYPE_AAAA && addr->addr.type == ESP_IPADDR_TYPE_V6) {
        return true;
    }
#endif
    return false;
}

static bool entry_has_addr_type(const mdns_cache_entry_t *entry, mdns_resolver_type_t type)
{
    if (!entry || (type != MDNS_RESOLVER_TYPE_A && type != MDNS_RESOLVER_TYPE_AAAA)) {
        return false;
    }

    for (const mdns_cache_addr_t *addr = entry->addr_list; addr; addr = addr->next) {
        if (cache_addr_matches_resolver_type(addr, type)) {
            return true;
        }
    }
    return false;
}

static bool resolver_matches_service_cache(const mdns_resolver_t *resolver, const mdns_cache_entry_t *entry,
                                           const mdns_service_cache_t *service, bool goodbye)
{
    if (!resolver || !entry || resolver->state != RESOLVER_RUNNING) {
        return false;
    }

    switch (resolver->type) {
    case MDNS_RESOLVER_TYPE_PTR:
        return service && resolver->notifier.ptr
               && mdns_utils_str_case_equal(resolver->service, service->service)
               && mdns_utils_str_case_equal(resolver->proto, service->proto)
               && (goodbye || (mdns_utils_str_null_or_empty(resolver->subtype) ? service->ptr_present
                               : mdns_priv_cache_service_find_subtype(service, resolver->subtype) != NULL));
    case MDNS_RESOLVER_TYPE_SRV:
        return service && service->srv_present && resolver->notifier.srv
               && mdns_utils_str_case_equal(resolver->instance, service->instance_name)
               && mdns_utils_str_case_equal(resolver->service, service->service)
               && mdns_utils_str_case_equal(resolver->proto, service->proto);
    case MDNS_RESOLVER_TYPE_TXT:
        return service && service->txt_present && resolver->notifier.txt
               && mdns_utils_str_case_equal(resolver->instance, service->instance_name)
               && mdns_utils_str_case_equal(resolver->service, service->service)
               && mdns_utils_str_case_equal(resolver->proto, service->proto);
    case MDNS_RESOLVER_TYPE_A:
    case MDNS_RESOLVER_TYPE_AAAA:
        return resolver->notifier.addr
               && mdns_utils_str_case_equal(resolver->hostname, entry->hostname)
               && (goodbye || entry_has_addr_type(entry, resolver->type));
    default:
        ESP_LOGE(TAG, "Invalid resolver type: %d", resolver->type);
        return false;
    }
}

static void resolver_item_free(mdns_resolver_t *resolver)
{
    if (!resolver) {
        return;
    }

    mdns_mem_free(resolver->instance);
    mdns_mem_free(resolver->service);
    mdns_mem_free(resolver->proto);
    mdns_mem_free(resolver->subtype);
    mdns_mem_free(resolver->hostname);
    mdns_mem_free(resolver);
}

void mdns_priv_resolver_free(void)
{
    while (s_resolver) {
        mdns_resolver_t *resolver = s_resolver;
        s_resolver = resolver->next;
        resolver_item_free(resolver);
    }
    s_resolver = NULL;
}

static void resolver_send_question(char *instance, char *service, char *proto, char *subtype, uint16_t record_type,
                                   mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_search_once_t search = {
        .type = record_type,
        .unicast = record_type != MDNS_TYPE_PTR,
        .instance = instance,
        .service = service,
        .proto = proto,
        .subtype = subtype,
    };
    mdns_priv_query_send(&search, mdns_if, ip_protocol);
}

static void resolver_send(mdns_resolver_t *resolver, mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol)
{
    if (!resolver || resolver->state != RESOLVER_RUNNING) {
        return;
    }

    uint16_t record_type = 0;
    switch (resolver->type) {
    case MDNS_RESOLVER_TYPE_PTR:
        record_type = MDNS_TYPE_PTR;
        break;
    case MDNS_RESOLVER_TYPE_SRV:
        record_type = MDNS_TYPE_SRV;
        break;
    case MDNS_RESOLVER_TYPE_TXT:
        record_type = MDNS_TYPE_TXT;
        break;
    case MDNS_RESOLVER_TYPE_A:
        record_type = MDNS_TYPE_A;
        break;
    case MDNS_RESOLVER_TYPE_AAAA:
        record_type = MDNS_TYPE_AAAA;
        break;
    default:
        ESP_LOGE(TAG, "Invalid resolver type: %d", resolver->type);
        return;
    }

    if (resolver->type == MDNS_RESOLVER_TYPE_A || resolver->type == MDNS_RESOLVER_TYPE_AAAA) {
        // mDNS query uses `instance` to hold hostname for A and AAAA queries.
        resolver_send_question(resolver->hostname, NULL, NULL, NULL, record_type, mdns_if, ip_protocol);
    } else {
        resolver_send_question(resolver->instance, resolver->service, resolver->proto, resolver->subtype,
                               record_type, mdns_if, ip_protocol);
    }
}

static mdns_resolver_t *resolver_init(const char *instance, const char *service, const char *proto, const char *subtype,
                                      const char *hostname, mdns_resolver_type_t type)
{
    mdns_resolver_t *resolver = (mdns_resolver_t *)mdns_mem_calloc(1, sizeof(mdns_resolver_t));
    if (!resolver) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    resolver->type = type;
    resolver->state = RESOLVER_INIT;

    if (!mdns_utils_str_null_or_empty(instance)) {
        resolver->instance = mdns_mem_strndup(instance, MDNS_NAME_MAX_LEN);
        if (!resolver->instance) {
            HOOK_MALLOC_FAILED;
            resolver_item_free(resolver);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(service)) {
        resolver->service = mdns_mem_strndup(service, MDNS_NAME_MAX_LEN);
        if (!resolver->service) {
            HOOK_MALLOC_FAILED;
            resolver_item_free(resolver);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(proto)) {
        resolver->proto = mdns_mem_strndup(proto, MDNS_NAME_MAX_LEN);
        if (!resolver->proto) {
            HOOK_MALLOC_FAILED;
            resolver_item_free(resolver);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(subtype)) {
        resolver->subtype = mdns_mem_strndup(subtype, MDNS_NAME_MAX_LEN);
        if (!resolver->subtype) {
            HOOK_MALLOC_FAILED;
            resolver_item_free(resolver);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(hostname)) {
        resolver->hostname = mdns_mem_strndup(hostname, MDNS_NAME_MAX_LEN);
        if (!resolver->hostname) {
            HOOK_MALLOC_FAILED;
            resolver_item_free(resolver);
            return NULL;
        }
    }

    return resolver;
}

static void resolver_start(mdns_resolver_t *resolver)
{
    if (!resolver || resolver->state != RESOLVER_INIT) {
        return;
    }
    resolver->state = RESOLVER_RUNNING;
    (void)mdns_priv_cache_notify_resolver(resolver);
    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
            resolver_send(resolver, (mdns_if_t) interface_idx, (mdns_ip_protocol_t) protocol_idx);
        }
    }
}

static void resolver_finish(mdns_resolver_t *resolver)
{
    if (!resolver) {
        return;
    }

    mdns_resolver_t **link = &s_resolver;
    bool found = false;
    while (*link) {
        mdns_resolver_t *it = *link;
        if (it == resolver) {
            resolver->state = RESOLVER_OFF;
            *link = it->next;
            found = true;
            break;
        }
        link = &it->next;
    }

    if (found) {
        resolver_item_free(resolver);
        mdns_priv_cache_trim_unsubscribed_records(esp_timer_get_time());
    }
}

static esp_err_t send_resolver_action(mdns_action_type_t type, mdns_resolver_t *resolver)
{
    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.resolver_add_end.resolver = resolver;

    if (!mdns_priv_queue_action(action)) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static mdns_result_t *build_result(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                   const char *subtype, bool goodbye, mdns_resolver_type_t type)
{
    esp_err_t ret __attribute__((unused)) = ESP_OK;
    if (!entry || (!service && type != MDNS_RESOLVER_TYPE_A && type != MDNS_RESOLVER_TYPE_AAAA)) {
        return NULL;
    }

    mdns_result_t *result = (mdns_result_t *)mdns_mem_calloc(1, sizeof(mdns_result_t));
    if (!result) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    result->esp_netif = entry->esp_netif;
    result->ip_protocol = entry->ip_protocol;

    // Clone instance, service, and proto for PTR/SRV/TXT resolvers
    if (service) {
        result->instance_name = mdns_mem_strdup(service->instance_name);
        ESP_GOTO_ON_FALSE(result->instance_name, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate instance name");

        result->service_type = mdns_mem_strdup(service->service);
        ESP_GOTO_ON_FALSE(result->service_type, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate service type");

        result->proto = mdns_mem_strdup(service->proto);
        ESP_GOTO_ON_FALSE(result->proto, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate protocol");
    }

    switch (type) {
    case MDNS_RESOLVER_TYPE_PTR:
        const bool has_subtype = !mdns_utils_str_null_or_empty(subtype);
        const mdns_cache_subtype_t *subtype_cache = has_subtype ? mdns_priv_cache_service_find_subtype(service, subtype) : NULL;
        ESP_GOTO_ON_FALSE(goodbye || (has_subtype ? subtype_cache != NULL : service->ptr_present),
                          ESP_ERR_INVALID_STATE, error, TAG, "PTR cache unavailable");

        if (has_subtype) {
            result->subtype = mdns_mem_strdup(subtype);
            ESP_GOTO_ON_FALSE(result->subtype, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate subtype");
        }
        result->ttl = goodbye ? 0 : (has_subtype ? subtype_cache->ttl : service->ptr_ttl);
        break;
    case MDNS_RESOLVER_TYPE_SRV:
        if (entry->hostname) {
            result->hostname = mdns_mem_strdup(entry->hostname);
            ESP_GOTO_ON_FALSE(result->hostname, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate hostname");
        }
        result->port = service->port;
        result->priority = service->priority;
        result->weight = service->weight;
        result->srv_ttl = goodbye ? 0 : service->srv_ttl;
        break;
    case MDNS_RESOLVER_TYPE_TXT:
        ESP_GOTO_ON_FALSE(mdns_priv_cache_copy_txt(service->txt_list, &result->txt, &result->txt_value_len, &result->txt_count),
                          ESP_ERR_NO_MEM, error, TAG, "Failed to allocate TXT list");
        result->txt_ttl = goodbye ? 0 : service->txt_ttl;
        break;
    case MDNS_RESOLVER_TYPE_A:
    case MDNS_RESOLVER_TYPE_AAAA:
        result->hostname = mdns_mem_strdup(entry->hostname);
        ESP_GOTO_ON_FALSE(result->hostname, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate hostname");

        // On goodbye, the TTL=0 address is already removed from address list.
        for (const mdns_cache_addr_t *cached_addr = entry->addr_list; cached_addr; cached_addr = cached_addr->next) {
            if (!cache_addr_matches_resolver_type(cached_addr, type)) {
                continue;
            }

            mdns_ip_addr_t *new_addr = mdns_mem_calloc(1, sizeof(mdns_ip_addr_t));
            ESP_GOTO_ON_FALSE(new_addr, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate address");

            new_addr->addr = cached_addr->addr;
            new_addr->ttl = cached_addr->ttl;
            new_addr->next = result->addr;
            result->addr = new_addr;
        }
        break;
    }

    return result;

error:
    HOOK_MALLOC_FAILED;
    mdns_results_free(result);
    return NULL;
}

static bool resolver_notify(mdns_resolver_t *resolver, const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                            bool goodbye)
{
    if (!resolver || !entry) {
        return false;
    }

    mdns_result_t *result = NULL;
    switch (resolver->type) {
    case MDNS_RESOLVER_TYPE_PTR:
        if (!resolver->notifier.ptr) {
            return false;
        }

        result = build_result(entry, service, resolver->subtype, goodbye, resolver->type);
        if (!result) {
            return false;
        }

        resolver->notifier.ptr(result);
        return true;
    case MDNS_RESOLVER_TYPE_SRV:
        if (!service || !resolver->notifier.srv) {
            return false;
        }

        result = build_result(entry, service, NULL, goodbye, resolver->type);
        if (!result) {
            return false;
        }

        resolver->notifier.srv(result);
        return true;
    case MDNS_RESOLVER_TYPE_TXT:
        if (!service || !resolver->notifier.txt) {
            return false;
        }

        result = build_result(entry, service, NULL, goodbye, resolver->type);
        if (!result) {
            return false;
        }

        resolver->notifier.txt(result);
        return true;
    case MDNS_RESOLVER_TYPE_A:
    case MDNS_RESOLVER_TYPE_AAAA:
        if (!resolver->notifier.addr) {
            return false;
        }

        result = build_result(entry, NULL, NULL, goodbye, resolver->type);
        if (!result) {
            return false;
        }

        resolver->notifier.addr(result);
        return true;
    default:
        ESP_LOGE(TAG, "Invalid resolver type: %d", resolver->type);
        return false;
    }
}

void mdns_priv_resolver_action(mdns_action_t *action, mdns_action_subtype_t type)
{
    if (type == ACTION_RUN) {
        switch (action->type) {
        case ACTION_RESOLVER_START:
            resolver_start(action->data.resolver_add_end.resolver);
            break;
        case ACTION_RESOLVER_END:
            resolver_finish(action->data.resolver_add_end.resolver);
            break;
        default:
            abort();
        }
    } else if (type == ACTION_CLEANUP) {
        switch (action->type) {
        case ACTION_RESOLVER_START:
        case ACTION_RESOLVER_END:
            // Resolver actions do not own the resolver. If a queued action is
            // discarded during shutdown, the linked resolver is released by
            // mdns_priv_resolver_free().
            break;
        default:
            abort();
        }
    }
}

void mdns_priv_resolver_send_by_ip_protocol(mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol)
{
    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        resolver_send(it, mdns_if, ip_protocol);
    }
}

mdns_resolver_t *mdns_priv_resolver_find(const char *instance, const char *service, const char *proto,
                                         const char *subtype, mdns_resolver_type_t type)
{
    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->type != type || it->state != RESOLVER_RUNNING) {
            continue;
        }

        switch (type) {
        case MDNS_RESOLVER_TYPE_PTR:
            if (mdns_utils_str_case_equal(it->service, service) && mdns_utils_str_case_equal(it->proto, proto)
                    && mdns_utils_str_case_equal_nullable(it->subtype, subtype)) {
                return it;
            }
            break;
        case MDNS_RESOLVER_TYPE_SRV:
        case MDNS_RESOLVER_TYPE_TXT:
            if (mdns_utils_str_case_equal(it->instance, instance) && mdns_utils_str_case_equal(it->service, service)
                    && mdns_utils_str_case_equal(it->proto, proto)) {
                return it;
            }
            break;
        case MDNS_RESOLVER_TYPE_A:
        case MDNS_RESOLVER_TYPE_AAAA:
            if (mdns_utils_str_case_equal(it->hostname, instance)) {
                return it;
            }
            break;
        default:
            ESP_LOGE(TAG, "Invalid resolver type: %d", it->type);
            break;
        }
    }
    return NULL;
}

bool mdns_priv_resolver_has_service(const char *instance, const char *service, const char *proto)
{
    for (const mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->state == RESOLVER_RUNNING
                && mdns_utils_str_case_equal(it->service, service)
                && mdns_utils_str_case_equal(it->proto, proto)
                && (it->type == MDNS_RESOLVER_TYPE_PTR || mdns_utils_str_case_equal(it->instance, instance))) {
            return true;
        }
    }
    return false;
}

bool mdns_priv_resolver_has_hostname(const char *hostname, mdns_resolver_type_t type)
{
    if (mdns_utils_str_null_or_empty(hostname) || (type != MDNS_RESOLVER_TYPE_A && type != MDNS_RESOLVER_TYPE_AAAA)) {
        return false;
    }

    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->state != RESOLVER_OFF && it->type == type && mdns_utils_str_case_equal(it->hostname, hostname)) {
            return true;
        }
    }
    return false;
}

mdns_resolver_t *mdns_priv_resolver_find_ptr(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    if (!name || (type != MDNS_TYPE_SRV && type != MDNS_TYPE_TXT && type != MDNS_TYPE_A && type != MDNS_TYPE_AAAA)) {
        return NULL;
    }

    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->state != RESOLVER_RUNNING || it->type != MDNS_RESOLVER_TYPE_PTR) {
            continue;
        }

        switch (type) {
        case MDNS_TYPE_SRV:
        case MDNS_TYPE_TXT:
            if (mdns_utils_str_case_equal(it->service, name->service) && mdns_utils_str_case_equal(it->proto, name->proto)) {
                return it;
            }
            break;
        case MDNS_TYPE_A:
        case MDNS_TYPE_AAAA:
            if (mdns_priv_cache_host_has_service(name->host, mdns_priv_get_esp_netif(tcpip_if),
                                                 ip_protocol, it->service, it->proto)) {
                return it;
            }
            break;
        }
    }
    return NULL;
}

mdns_cache_record_mask_t mdns_priv_resolver_update_from_cache(const mdns_cache_entry_t *entry,
                                                              const mdns_service_cache_t *service,
                                                              mdns_cache_record_mask_t record_mask)
{
    mdns_cache_record_mask_t completed_records = record_mask;

    if (!entry || record_mask == 0) {
        return 0;
    }

    for (mdns_resolver_t *resolver = s_resolver; resolver; resolver = resolver->next) {
        // Checks if the cache in resolver's record type is to be notified.
        mdns_cache_record_type_t record_type = resolver_type_to_record_type(resolver);
        if (!(record_mask & (mdns_cache_record_mask_t)record_type)) {
            continue;
        }
        // Checks if the resolver matches the service cache.
        if (!resolver_matches_service_cache(resolver, entry, service, false)) {
            continue;
        }
        if (resolver->type == MDNS_RESOLVER_TYPE_PTR && !mdns_utils_str_null_or_empty(resolver->subtype)) {
            const mdns_cache_subtype_t *subtype_entry = mdns_priv_cache_service_find_subtype(service, resolver->subtype);
            if (!subtype_entry || !subtype_entry->pending_sync) {
                continue;
            }
        }

        if (!resolver_notify(resolver, entry, service, false)) {
            completed_records &= ~(mdns_cache_record_mask_t)record_type;
        }
    }

    return completed_records;
}

bool mdns_priv_resolver_notify_from_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                          mdns_resolver_t *resolver)
{
    if (!entry || !resolver || resolver->state != RESOLVER_RUNNING) {
        return false;
    }

    if (!resolver_matches_service_cache(resolver, entry, service, false)) {
        return true;
    }

    return resolver_notify(resolver, entry, service, false);
}

bool mdns_priv_resolver_notify_goodbye_from_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                  mdns_cache_record_mask_t record_mask, const char *subtype)
{
    bool notified = true;

    if (!entry || record_mask == 0) {
        return false;
    }

    for (mdns_resolver_t *resolver = s_resolver; resolver; resolver = resolver->next) {
        // Checks if the cache in resolver's record type is to be notified.
        mdns_cache_record_type_t record_type = resolver_type_to_record_type(resolver);
        if (!(record_mask & (mdns_cache_record_mask_t)record_type)) {
            continue;
        }
        // Expired base PTRs are already absent. Match identity, not record presence.
        if (!resolver_matches_service_cache(resolver, entry, service, true)) {
            continue;
        }
        if (resolver->type == MDNS_RESOLVER_TYPE_PTR && !mdns_utils_str_case_equal_nullable(resolver->subtype, subtype)) {
            continue;
        }

        notified &= resolver_notify(resolver, entry, service, true);
    }

    return notified;
}

static mdns_resolver_t *resolver_new(const char *instance, const char *service, const char *proto, const char *subtype,
                                     const char *hostname, mdns_resolver_type_t type, void *notifier)
{
    mdns_resolver_t *resolver = NULL;

    if (!mdns_priv_is_server_init() || !notifier) {
        return NULL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > 63) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return NULL;
    }

    resolver = resolver_init(instance, service, proto, subtype, hostname, type);
    if (!resolver) {
        return NULL;
    }

    switch (type) {
    case MDNS_RESOLVER_TYPE_PTR:
        resolver->notifier.ptr = (mdns_ptr_resolver_notify_t)notifier;
        break;
    case MDNS_RESOLVER_TYPE_SRV:
        resolver->notifier.srv = (mdns_srv_resolver_notify_t)notifier;
        break;
    case MDNS_RESOLVER_TYPE_TXT:
        resolver->notifier.txt = (mdns_txt_resolver_notify_t)notifier;
        break;
    case MDNS_RESOLVER_TYPE_A:
    case MDNS_RESOLVER_TYPE_AAAA:
        resolver->notifier.addr = (mdns_addr_resolver_notify_t)notifier;
        break;
    default:
        resolver_item_free(resolver);
        return NULL;
    }

    mdns_priv_service_lock();

    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->state != RESOLVER_OFF && resolvers_match(it, resolver)) {
            ESP_LOGW(TAG, "Resolver already exists: %s%s%s.%s %s",
                     resolver->instance ? resolver->instance : "",
                     resolver->instance ? "." : "",
                     resolver->service, resolver->proto,
                     resolver->subtype ? resolver->subtype : "");
            goto error;
        }
    }

    if (send_resolver_action(ACTION_RESOLVER_START, resolver) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send resolver start action");
        goto error;
    }

    resolver->next = s_resolver;
    s_resolver = resolver;

    mdns_priv_service_unlock();
    return resolver;

error:
    mdns_priv_service_unlock();
    resolver_item_free(resolver);
    return NULL;
}

/**
 * @defgroup MDNS_PUBCLIC_API
 */
mdns_resolver_t *mdns_ptr_resolver_new(const char *service, const char *proto, const char *subtype,
                                       mdns_ptr_resolver_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }
    return resolver_new(NULL, service, proto, subtype, NULL, MDNS_RESOLVER_TYPE_PTR, notifier);
}

mdns_resolver_t *mdns_srv_resolver_new(const char *instance, const char *service, const char *proto,
                                       mdns_srv_resolver_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }
    return resolver_new(instance, service, proto, NULL, NULL, MDNS_RESOLVER_TYPE_SRV, notifier);
}

mdns_resolver_t *mdns_txt_resolver_new(const char *instance, const char *service, const char *proto,
                                       mdns_txt_resolver_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }
    return resolver_new(instance, service, proto, NULL, NULL, MDNS_RESOLVER_TYPE_TXT, notifier);
}

#ifdef CONFIG_LWIP_IPV4
mdns_resolver_t *mdns_a_resolver_new(const char *hostname, mdns_addr_resolver_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(hostname)) {
        return NULL;
    }
    return resolver_new(NULL, NULL, NULL, NULL, hostname, MDNS_RESOLVER_TYPE_A, notifier);
}
#endif

#ifdef CONFIG_LWIP_IPV6
mdns_resolver_t *mdns_aaaa_resolver_new(const char *hostname, mdns_addr_resolver_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(hostname)) {
        return NULL;
    }
    return resolver_new(NULL, NULL, NULL, NULL, hostname, MDNS_RESOLVER_TYPE_AAAA, notifier);
}
#endif

esp_err_t mdns_resolver_delete(mdns_resolver_t *resolver)
{
    bool found = false;
    esp_err_t err = ESP_OK;

    if (!resolver) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!mdns_priv_is_server_init()) {
        return ESP_ERR_INVALID_STATE;
    }

    mdns_priv_service_lock();
    mdns_resolver_state_t prev_state = resolver->state;
    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->state != RESOLVER_OFF && it == resolver) {
            resolver->state = RESOLVER_OFF;
            found = true;
            break;
        }
    }

    if (!found) {
        mdns_priv_service_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    err = send_resolver_action(ACTION_RESOLVER_END, resolver);
    if (err != ESP_OK) {
        resolver->state = prev_state;
    }

    mdns_priv_service_unlock();
    return err;
}
