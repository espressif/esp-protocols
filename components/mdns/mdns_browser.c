/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "mdns_private.h"
#include "mdns_browser.h"
#include "mdns_mem_caps.h"
#include "mdns_debug.h"
#include "mdns_utils.h"
#include "mdns_querier.h"
#include "mdns_responder.h"
#include "mdns_netif.h"
#include "mdns_service.h"
#include "esp_log.h"
#include "esp_check.h"
#include "mdns_cache.h"

#define MDNS_CACHE_RECORD_BROWSE_SHARED (mdns_cache_record_mask_t) \
                                        (MDNS_CACHE_RECORD_SRV | MDNS_CACHE_RECORD_TXT | MDNS_CACHE_RECORD_ADDR)

static const char *TAG = "mdns_browser";

static mdns_browse_t *s_browse;
static mdns_browse_record_t *s_browse_record;

static void browse_record_send(mdns_browse_record_t *browse_record, mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol);

/**
 * @brief  Browse action
 */
static esp_err_t send_browse_action(mdns_action_type_t type, mdns_browse_t *browse)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));

    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.browse_add.browse = browse;
    if (!mdns_priv_queue_action(action)) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief  Free a browse item (Not free the list).
 */
static void browse_item_free(mdns_browse_t *browse)
{
    if (!browse) {
        return;
    }

    mdns_mem_free(browse->service);
    mdns_mem_free(browse->proto);
    mdns_mem_free(browse->subtype);
    mdns_mem_free(browse);
}

/**
 * @brief  Send PTR query packet to all available interfaces for browsing.
 */
static void browse_send(mdns_browse_t *browse, mdns_if_t interface, mdns_ip_protocol_t ip_protocol)
{
    // Using search once for sending the PTR query
    mdns_search_once_t search = {0};

    search.instance = NULL;
    search.service = browse->service;
    search.proto = browse->proto;
    search.subtype = browse->subtype;
    search.type = MDNS_TYPE_PTR;
    search.unicast = false;
    search.result = NULL;
    search.next = NULL;
    mdns_priv_query_send(&search, interface, ip_protocol);
}

void mdns_priv_browse_send_by_ip_protocol(mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol)
{
    for (mdns_browse_t *browse = s_browse; browse; browse = browse->next) {
        if (browse->state == BROWSE_RUNNING) {
            browse_send(browse, mdns_if, ip_protocol);
        }
    }
    for (mdns_browse_record_t *browse_record = s_browse_record; browse_record; browse_record = browse_record->next) {
        browse_record_send(browse_record, mdns_if, ip_protocol);
    }
}

void mdns_priv_browse_free(void)
{
    while (s_browse) {
        mdns_browse_t *b = s_browse;
        s_browse = s_browse->next;
        browse_item_free(b);
    }
}

/**
 * @brief Check if a browse matches a given service and proto.
 */
static bool browse_matches_identity(const mdns_browse_t *browse, const char *service, const char *proto)
{
    return browse && mdns_utils_str_case_equal(browse->service, service) && mdns_utils_str_case_equal(browse->proto, proto);
}

/**
 * @brief Check if a browse matches a given service, proto, and possible subtype.
 */
static bool browse_matches_identity_with_subtype(const mdns_browse_t *browse, const char *service, const char *proto, const char *subtype)
{
    return browse_matches_identity(browse, service, proto) && mdns_utils_str_case_equal_nullable(browse->subtype, subtype);
}

/**
 * @brief Get the non-BROWSE_OFF browse item with the given service, proto, and subtype.
 */
static mdns_browse_t *get_non_off_browse_item(const char *service, const char *proto, const char *subtype)
{
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_OFF && browse_matches_identity_with_subtype(it, service, proto, subtype)) {
            return it;
        }
    }
    return NULL;
}

bool mdns_priv_browse_has_service(const char *service, const char *proto)
{
    for (const mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_RUNNING && browse_matches_identity(it, service, proto)) {
            return true;
        }
    }
    return false;
}

bool mdns_priv_browse_has_subtype(const char *service, const char *proto, const char *subtype)
{
    for (const mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_RUNNING && browse_matches_identity_with_subtype(it, service, proto, subtype)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief  Remove and free the browse from browse linked list
 */
static void browse_finish(mdns_browse_t *browse)
{
    if (!browse) {
        return;
    }

    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it == browse) {
            queueDetach(mdns_browse_t, s_browse, it);
            browse_item_free(it);
            mdns_priv_cache_trim_unsubscribed_records(esp_timer_get_time());
            return;
        }
    }
}

/**
 * @brief  Allocate new browse structure
 */
static mdns_browse_t *browse_init(const char *service, const char *proto, const char *subtype, mdns_browse_notify_t notifier)
{
    mdns_browse_t *browse = (mdns_browse_t *)mdns_mem_malloc(sizeof(mdns_browse_t));

    if (!browse) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }
    memset(browse, 0, sizeof(mdns_browse_t));

    browse->state = BROWSE_INIT;
    if (!mdns_utils_str_null_or_empty(service)) {
        browse->service = mdns_mem_strndup(service, MDNS_NAME_BUF_LEN - 1);
        if (!browse->service) {
            browse_item_free(browse);
            HOOK_MALLOC_FAILED;
            return NULL;
        }
    }
    if (!mdns_utils_str_null_or_empty(proto)) {
        browse->proto = mdns_mem_strndup(proto, MDNS_NAME_BUF_LEN - 1);
        if (!browse->proto) {
            browse_item_free(browse);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(subtype)) {
        browse->subtype = mdns_mem_strndup(subtype, MDNS_NAME_BUF_LEN - 1);
        if (!browse->subtype) {
            browse_item_free(browse);
            return NULL;
        }
    }

    browse->notifier = notifier;
    return browse;
}

/**
 * @brief  Send initial PTR queries for a registered browse.
 */
static void browse_start(mdns_browse_t *browse)
{
    if (!browse || browse->state != BROWSE_INIT) {
        return;
    }
    browse->state = BROWSE_RUNNING;
    mdns_priv_cache_notify_browse(browse);
    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
            browse_send(browse, (mdns_if_t) interface_idx, (mdns_ip_protocol_t) protocol_idx);
        }
    }
}

/**
 * @brief  Called from packet parser to find matching running search
 *
 * @note Called from the mDNS service task while the service lock is held.
 *       The returned browse is an active cache node that the parser may update.
 */
mdns_browse_t *mdns_priv_browse_find_ptr(mdns_name_t *name)
{
    if (mdns_utils_str_null_or_empty(name->service) || mdns_utils_str_null_or_empty(name->proto)) {
        return NULL;
    }

    for (mdns_browse_t *b = s_browse; b; b = b->next) {
        bool subtype_matches = name->sub ? mdns_utils_str_case_equal(name->host, b->subtype) :
                               mdns_utils_str_null_or_empty(b->subtype);

        if (b->state == BROWSE_RUNNING && !strcasecmp(name->service, b->service) && !strcasecmp(name->proto, b->proto)
                && subtype_matches) {
            return b;
        }
    }
    return NULL;
}

mdns_browse_t *mdns_priv_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_browse_t *b = s_browse;
    // For browse, we only care about the SRV, TXT, A and AAAA
    if (type != MDNS_TYPE_SRV && type != MDNS_TYPE_A && type != MDNS_TYPE_AAAA && type != MDNS_TYPE_TXT) {
        return NULL;
    }

    while (b) {
        if (b->state != BROWSE_RUNNING) {
            b = b->next;
            continue;
        }

        if (type == MDNS_TYPE_SRV || type == MDNS_TYPE_TXT) {
            if (strcasecmp(name->service, b->service)
                    || strcasecmp(name->proto, b->proto)) {
                b = b->next;
                continue;
            }
            return b;
        } else if (type == MDNS_TYPE_A || type == MDNS_TYPE_AAAA) {
            if (mdns_priv_cache_host_has_service(name->host, mdns_priv_get_esp_netif(tcpip_if),
                                                 ip_protocol, b->service, b->proto)) {
                return b;
            }
            b = b->next;
            continue;
        }
    }
    return NULL;
}

void mdns_priv_browse_action(mdns_action_t *action, mdns_action_subtype_t type)
{
    if (type == ACTION_RUN) {
        switch (action->type) {
        case ACTION_BROWSE_START:
            browse_start(action->data.browse_add.browse);
            break;
        case ACTION_BROWSE_END:
            browse_finish(action->data.browse_add.browse);
            break;
        default:
            abort();
        }
        return;
    }
    if (type == ACTION_CLEANUP) {
        switch (action->type) {
        case ACTION_BROWSE_START:
        case ACTION_BROWSE_END:
            // Browse actions do not own the browse item.
            // If a queued action is discarded during shutdown, the linked browse item is released by mdns_priv_browse_free().
            break;
        default:
            abort();
        }
        return;
    }

}

/**
 * @brief Check if a running browse `_service._proto` matches a service cache `_service._proto`.
 */
static bool browse_matches_service_cache(const mdns_browse_t *browse, const mdns_service_cache_t *service)
{
    return browse && service && browse->state == BROWSE_RUNNING && browse->notifier
           && mdns_utils_str_case_equal(browse->service, service->service)
           && mdns_utils_str_case_equal(browse->proto, service->proto);
}

/**
 * @brief Check if a running browse matches a service cache with service name, proto, and possible subtype.
 *        If subtype node is found in cache and `out_subtype` is valid, the subtype node in cache will be passed to it.
 */
static bool browse_matches_service_cache_with_subtype(const mdns_browse_t *browse, const mdns_service_cache_t *service, const mdns_cache_subtype_t **out_subtype)
{
    const mdns_cache_subtype_t* subtype_node = NULL;
    if (out_subtype) {
        *out_subtype = NULL;
    }

    if (!browse_matches_service_cache(browse, service)) {
        return false;
    }

    if (mdns_utils_str_null_or_empty(browse->subtype)) {
        return service->ptr_present;
    }

    subtype_node = mdns_priv_cache_service_find_subtype(service, browse->subtype);
    if (out_subtype) {
        *out_subtype = subtype_node;
    }
    return subtype_node != NULL;
}

/**
 * @brief Build and sync a temporary result for a browse from cache.
 */
static bool browse_build_and_notify_temp_result(mdns_browse_t *browse, const mdns_cache_entry_t *entry,
                                                const mdns_service_cache_t *service, const mdns_cache_subtype_t *subtype,
                                                bool goodbye)
{
    mdns_result_t *result = NULL;
    esp_err_t ret = mdns_priv_service_cache_to_result(entry, service, &result);
    if (ret != ESP_OK) {
        return false;
    }

    if (!mdns_utils_str_null_or_empty(browse->subtype)) {
        if (subtype) {
            result->ttl = subtype->ttl;
        }
        result->subtype = mdns_mem_strdup(browse->subtype);
        if (!result->subtype) {
            HOOK_MALLOC_FAILED;
            mdns_priv_query_results_free(result);
            return false;
        }
    }

    result->next = NULL;
    if (goodbye) {
        result->ttl = 0;
    }

    DBG_BROWSE_RESULTS(result, browse);
    if (browse->notifier) {
        browse->notifier(result);
    }

    mdns_priv_query_results_free(result);
    return true;
}

static bool browse_need_update(const mdns_browse_t *browse, const mdns_service_cache_t *service,
                               const mdns_cache_subtype_t *subtype, mdns_cache_record_mask_t records)
{
    if (!browse || !service) {
        return false;
    }

    // SRV, TXT, A, AAAA changed, notify all browses with idential service and proto.
    if (records & MDNS_CACHE_RECORD_BROWSE_SHARED) {
        return true;
    }

    // Normal PTR update with no subtype, only notify no-subtype browses.
    if (mdns_utils_str_null_or_empty(browse->subtype)) {
        return (records & MDNS_CACHE_RECORD_PTR) != 0;
    }

    // For subtype update, only notify if the subtype in cache is pending sync.
    if (!(records & MDNS_CACHE_RECORD_SUBTYPE)) {
        return false;
    }
    return subtype && subtype->pending_sync;
}

bool mdns_priv_browse_update_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                mdns_cache_record_mask_t records)
{
    if (!entry || !service) {
        return false;
    }

    bool updated = true;

    for (mdns_browse_t *browse = s_browse; browse; browse = browse->next) {
        const mdns_cache_subtype_t *subtype = NULL;
        if (!browse_matches_service_cache_with_subtype(browse, service, &subtype)) {
            continue;
        }
        if (!browse_need_update(browse, service, subtype, records)) {
            continue;
        }
        updated &= browse_build_and_notify_temp_result(browse, entry, service, subtype, false);
    }

    return updated;
}

bool mdns_priv_browse_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                mdns_browse_t *browse)
{
    const mdns_cache_subtype_t *subtype = NULL;

    if (!entry || !service || !browse) {
        return false;
    }

    if (!browse_matches_service_cache_with_subtype(browse, service, &subtype)) {
        return true;
    }

    return browse_build_and_notify_temp_result(browse, entry, service, subtype, false);
}

bool mdns_priv_browse_notify_ptr_goodbye_from_service_cache(const mdns_cache_entry_t *entry,
                                                            const mdns_service_cache_t *service,
                                                            const char *subtype)
{
    if (!entry || !service) {
        return false;
    }

    bool notified = true;

    for (mdns_browse_t *browse = s_browse; browse; browse = browse->next) {
        if (!browse_matches_service_cache(browse, service)) {
            continue;
        }

        if (!mdns_utils_str_case_equal_nullable(browse->subtype, subtype)) {
            continue;
        }

        notified &= browse_build_and_notify_temp_result(browse, entry, service, NULL, true);
    }

    return notified;
}


static mdns_cache_record_type_t browse_record_type_to_record_type(mdns_browse_record_t *browse_record)
{
    if (!browse_record) {
        return MDNS_CACHE_RECORD_INVALID;
    }

    switch (browse_record->type) {
    case MDNS_BROWSE_RECORD_TYPE_PTR:
        return mdns_utils_str_null_or_empty(browse_record->subtype) ? MDNS_CACHE_RECORD_PTR : MDNS_CACHE_RECORD_SUBTYPE;
    default:
        return MDNS_CACHE_RECORD_INVALID;
    }
}

static bool browse_records_match(const mdns_browse_record_t *a, const mdns_browse_record_t *b)
{
    if (!a || !b || a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case MDNS_BROWSE_RECORD_TYPE_PTR:
        return mdns_utils_str_case_equal(a->service, b->service)
               && mdns_utils_str_case_equal(a->proto, b->proto)
               && mdns_utils_str_case_equal_nullable(a->subtype, b->subtype);
    default:
        return false;
    }
}

static bool browse_record_matches_service_cache(const mdns_browse_record_t *browse_record, const mdns_cache_entry_t *entry,
                                                const mdns_service_cache_t *service, bool goodbye)
{
    if (!browse_record || !entry || !service || browse_record->state != BROWSE_RUNNING) {
        return false;
    }

    switch (browse_record->type) {
    case MDNS_BROWSE_RECORD_TYPE_PTR:
        return browse_record->notifier
               && mdns_utils_str_case_equal(browse_record->service, service->service)
               && mdns_utils_str_case_equal(browse_record->proto, service->proto)
               && (goodbye || (mdns_utils_str_null_or_empty(browse_record->subtype) ? service->ptr_present
                               : mdns_priv_cache_service_find_subtype(service, browse_record->subtype) != NULL));
    default:
        ESP_LOGE(TAG, "Invalid record browse type: %d", browse_record->type);
        return false;
    }
}

static void browse_record_item_free(mdns_browse_record_t *browse_record)
{
    if (!browse_record) {
        return;
    }

    mdns_mem_free(browse_record->service);
    mdns_mem_free(browse_record->proto);
    mdns_mem_free(browse_record->subtype);
    mdns_mem_free(browse_record);
}

void mdns_priv_browse_record_free(void)
{
    while (s_browse_record) {
        mdns_browse_record_t *browse_record = s_browse_record;
        s_browse_record = browse_record->next;
        browse_record_item_free(browse_record);
    }
    s_browse_record = NULL;
}

static void browse_record_send(mdns_browse_record_t *browse_record, mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol)
{
    if (!browse_record || browse_record->state != BROWSE_RUNNING) {
        return;
    }

    uint16_t record_type = 0;
    switch (browse_record->type) {
    case MDNS_BROWSE_RECORD_TYPE_PTR:
        record_type = MDNS_TYPE_PTR;
        break;
    default:
        ESP_LOGE(TAG, "Invalid record browse type: %d", browse_record->type);
        return;
    }

    mdns_search_once_t search = {
        .type = record_type,
        .unicast = record_type != MDNS_TYPE_PTR,
        .service = browse_record->service,
        .proto = browse_record->proto,
        .subtype = browse_record->subtype,
    };
    mdns_priv_query_send(&search, mdns_if, ip_protocol);
}

static mdns_browse_record_t *browse_record_init(const char *service, const char *proto, const char *subtype,
                                                mdns_browse_record_type_t type)
{
    mdns_browse_record_t *browse_record = (mdns_browse_record_t *)mdns_mem_calloc(1, sizeof(mdns_browse_record_t));
    if (!browse_record) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    browse_record->type = type;
    browse_record->state = BROWSE_INIT;

    if (!mdns_utils_str_null_or_empty(service)) {
        browse_record->service = mdns_mem_strndup(service, MDNS_NAME_MAX_LEN);
        if (!browse_record->service) {
            HOOK_MALLOC_FAILED;
            browse_record_item_free(browse_record);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(proto)) {
        browse_record->proto = mdns_mem_strndup(proto, MDNS_NAME_MAX_LEN);
        if (!browse_record->proto) {
            HOOK_MALLOC_FAILED;
            browse_record_item_free(browse_record);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(subtype)) {
        browse_record->subtype = mdns_mem_strndup(subtype, MDNS_NAME_MAX_LEN);
        if (!browse_record->subtype) {
            HOOK_MALLOC_FAILED;
            browse_record_item_free(browse_record);
            return NULL;
        }
    }

    return browse_record;
}

static void browse_record_start(mdns_browse_record_t *browse_record)
{
    if (!browse_record || browse_record->state != BROWSE_INIT) {
        return;
    }
    browse_record->state = BROWSE_RUNNING;
    (void)mdns_priv_cache_notify_browse_record(browse_record);
    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
            browse_record_send(browse_record, (mdns_if_t) interface_idx, (mdns_ip_protocol_t) protocol_idx);
        }
    }
}

static void browse_record_finish(mdns_browse_record_t *browse_record)
{
    if (!browse_record) {
        return;
    }

    mdns_browse_record_t **link = &s_browse_record;
    bool found = false;
    while (*link) {
        mdns_browse_record_t *it = *link;
        if (it == browse_record) {
            browse_record->state = BROWSE_OFF;
            *link = it->next;
            found = true;
            break;
        }
        link = &it->next;
    }

    if (found) {
        browse_record_item_free(browse_record);
        mdns_priv_cache_trim_unsubscribed_records(esp_timer_get_time());
    }
}

static esp_err_t send_browse_record_action(mdns_action_type_t type, mdns_browse_record_t *browse_record)
{
    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.browse_record_add_end.browse_record = browse_record;

    if (!mdns_priv_queue_action(action)) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static mdns_result_t *browse_record_build_ptr_result(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                     const char *subtype, bool goodbye)
{
    esp_err_t ret __attribute__((unused)) = ESP_OK;
    if (!entry || !service) {
        return NULL;
    }
    const bool has_subtype = !mdns_utils_str_null_or_empty(subtype);
    const mdns_cache_subtype_t *subtype_cache = has_subtype ? mdns_priv_cache_service_find_subtype(service, subtype) : NULL;
    if (!goodbye && (has_subtype ? subtype_cache == NULL : !service->ptr_present)) {
        return NULL;
    }

    mdns_result_t *result = (mdns_result_t *)mdns_mem_calloc(1, sizeof(mdns_result_t));
    if (!result) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    result->esp_netif = entry->esp_netif;
    result->ip_protocol = entry->ip_protocol;

    result->instance_name = mdns_mem_strdup(service->instance_name);
    ESP_GOTO_ON_FALSE(result->instance_name, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate instance name");

    result->service_type = mdns_mem_strdup(service->service);
    ESP_GOTO_ON_FALSE(result->service_type, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate service type");

    result->proto = mdns_mem_strdup(service->proto);
    ESP_GOTO_ON_FALSE(result->proto, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate protocol");

    if (has_subtype) {
        result->subtype = mdns_mem_strdup(subtype);
        ESP_GOTO_ON_FALSE(result->subtype, ESP_ERR_NO_MEM, error, TAG, "Failed to allocate subtype");
    }

    result->ttl = goodbye ? 0 : (has_subtype ? subtype_cache->ttl : service->ptr_ttl);
    return result;

error:
    HOOK_MALLOC_FAILED;
    mdns_priv_query_results_free(result);
    return NULL;
}

static bool browse_record_notify(mdns_browse_record_t *browse_record, const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                 bool goodbye)
{
    if (!browse_record || !entry || !service) {
        return false;
    }

    mdns_result_t *result = NULL;
    switch (browse_record->type) {
    case MDNS_BROWSE_RECORD_TYPE_PTR:
        if (!browse_record->notifier) {
            return false;
        }

        result = browse_record_build_ptr_result(entry, service, browse_record->subtype, goodbye);
        if (!result) {
            return false;
        }

        browse_record->notifier(result);
        mdns_priv_query_results_free(result);
        return true;
    default:
        ESP_LOGE(TAG, "Invalid record browse type: %d", browse_record->type);
        return false;
    }
}

void mdns_priv_browse_record_action(mdns_action_t *action, mdns_action_subtype_t type)
{
    if (type == ACTION_RUN) {
        switch (action->type) {
        case ACTION_BROWSE_RECORD_START:
            browse_record_start(action->data.browse_record_add_end.browse_record);
            break;
        case ACTION_BROWSE_RECORD_END:
            browse_record_finish(action->data.browse_record_add_end.browse_record);
            break;
        default:
            abort();
        }
    } else if (type == ACTION_CLEANUP) {
        switch (action->type) {
        case ACTION_BROWSE_RECORD_START:
        case ACTION_BROWSE_RECORD_END:
            // Record browse actions do not own the record browse. If a queued action is
            // discarded during shutdown, the linked record browse is released by
            // mdns_priv_browse_record_free().
            break;
        default:
            abort();
        }
    }
}

mdns_browse_record_t *mdns_priv_browse_record_find(const char *service, const char *proto, const char *subtype,
                                                   mdns_browse_record_type_t type)
{
    for (mdns_browse_record_t *it = s_browse_record; it; it = it->next) {
        if (it->type != type || it->state != BROWSE_RUNNING) {
            continue;
        }

        switch (type) {
        case MDNS_BROWSE_RECORD_TYPE_PTR:
            if (mdns_utils_str_case_equal(it->service, service) && mdns_utils_str_case_equal(it->proto, proto)
                    && mdns_utils_str_case_equal_nullable(it->subtype, subtype)) {
                return it;
            }
            break;
        default:
            ESP_LOGE(TAG, "Invalid record browse type: %d", it->type);
            break;
        }
    }
    return NULL;
}

bool mdns_priv_browse_record_has_service(const char *service, const char *proto)
{
    for (const mdns_browse_record_t *it = s_browse_record; it; it = it->next) {
        if (it->state == BROWSE_RUNNING && mdns_utils_str_case_equal(it->service, service) && mdns_utils_str_case_equal(it->proto, proto)) {
            return true;
        }
    }
    return false;
}

mdns_browse_record_t *mdns_priv_browse_record_find_ptr_cache_consumer(mdns_name_t *name, uint16_t type,
                                                                      mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    if (!name || (type != MDNS_TYPE_SRV && type != MDNS_TYPE_TXT && type != MDNS_TYPE_A && type != MDNS_TYPE_AAAA)) {
        return NULL;
    }

    for (mdns_browse_record_t *it = s_browse_record; it; it = it->next) {
        if (it->state != BROWSE_RUNNING || it->type != MDNS_BROWSE_RECORD_TYPE_PTR) {
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

mdns_cache_record_mask_t mdns_priv_browse_record_update_from_service_cache(const mdns_cache_entry_t *entry,
                                                                           const mdns_service_cache_t *service,
                                                                           mdns_cache_record_mask_t record_mask)
{
    mdns_cache_record_mask_t completed_records = record_mask;

    if (!entry || !service) {
        return 0;
    }

    for (mdns_browse_record_t *browse_record = s_browse_record; browse_record; browse_record = browse_record->next) {
        // Notify only changes to the subscribed record type.
        mdns_cache_record_type_t record_type = browse_record_type_to_record_type(browse_record);
        if (!(record_mask & (mdns_cache_record_mask_t)record_type)) {
            continue;
        }
        // Check the subscription identity before notifying.
        if (!browse_record_matches_service_cache(browse_record, entry, service, false)) {
            continue;
        }
        if (browse_record->type == MDNS_BROWSE_RECORD_TYPE_PTR && !mdns_utils_str_null_or_empty(browse_record->subtype)) {
            const mdns_cache_subtype_t *subtype_entry = mdns_priv_cache_service_find_subtype(service, browse_record->subtype);
            if (!subtype_entry || !subtype_entry->pending_sync) {
                continue;
            }
        }

        if (!browse_record_notify(browse_record, entry, service, false)) {
            completed_records &= ~(mdns_cache_record_mask_t)record_type;
        }
    }

    return completed_records;
}

bool mdns_priv_browse_record_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                       mdns_browse_record_t *browse_record)
{
    if (!entry || !service || !browse_record || browse_record->state != BROWSE_RUNNING) {
        return false;
    }

    if (!browse_record_matches_service_cache(browse_record, entry, service, false)) {
        return true;
    }

    return browse_record_notify(browse_record, entry, service, false);
}

bool mdns_priv_browse_record_notify_goodbye_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                               mdns_cache_record_mask_t record_mask, const char *subtype)
{
    bool notified = true;

    if (!entry || !service) {
        return false;
    }

    for (mdns_browse_record_t *browse_record = s_browse_record; browse_record; browse_record = browse_record->next) {
        // Notify only changes to the subscribed record type.
        mdns_cache_record_type_t record_type = browse_record_type_to_record_type(browse_record);
        if (!(record_mask & (mdns_cache_record_mask_t)record_type)) {
            continue;
        }
        // Expired base PTRs are already absent. Match identity, not record presence.
        if (!browse_record_matches_service_cache(browse_record, entry, service, true)) {
            continue;
        }
        if (browse_record->type == MDNS_BROWSE_RECORD_TYPE_PTR && !mdns_utils_str_case_equal_nullable(browse_record->subtype, subtype)) {
            continue;
        }

        notified &= browse_record_notify(browse_record, entry, service, true);
    }

    return notified;
}

/**
 * @defgroup MDNS_PUBCLIC_API
 */
mdns_browse_t *mdns_browse_new(const char *service, const char *proto, mdns_browse_notify_t notifier)
{
    return mdns_browse_new_with_subtype(service, proto, NULL, notifier);
}

mdns_browse_t *mdns_browse_new_with_subtype(const char *service, const char *proto, const char *subtype, mdns_browse_notify_t notifier)
{
    mdns_browse_t *browse = NULL;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > 63) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return NULL;
    }

    browse = browse_init(service, proto, subtype, notifier);
    if (!browse) {
        return NULL;
    }

    mdns_priv_service_lock();
    // Check if the browse already exists before sending action
    if (get_non_off_browse_item(service, proto, subtype)) {
        bool has_subtype = !mdns_utils_str_null_or_empty(subtype);
        ESP_LOGW(TAG, "Browse already exists: %s%s%s.%s", has_subtype ? subtype : "",
                 has_subtype ? "._sub." : "", browse->service, browse->proto);
        goto error;
    }

    if (send_browse_action(ACTION_BROWSE_START, browse) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send browse start action");
        goto error;
    }

    browse->next = s_browse;
    s_browse = browse;
    mdns_priv_service_unlock();
    return browse;

error:
    browse_item_free(browse);
    mdns_priv_service_unlock();
    return NULL;
}

esp_err_t mdns_browse_delete(const char *service, const char *proto)
{
    return mdns_browse_delete_with_subtype(service, proto, NULL);
}

esp_err_t mdns_browse_delete_with_subtype(const char *service, const char *proto, const char *subtype)
{
    mdns_browse_t *browse = NULL;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_FAIL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > 63) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return ESP_FAIL;
    }

    mdns_priv_service_lock();
    browse = get_non_off_browse_item(service, proto, subtype);
    if (!browse) {
        mdns_priv_service_unlock();
        return ESP_FAIL;
    }

    mdns_browse_state_t prev_state = browse->state;
    browse->state = BROWSE_OFF;

    if (send_browse_action(ACTION_BROWSE_END, browse) != ESP_OK) {
        browse->state = prev_state;
        mdns_priv_service_unlock();
        return ESP_ERR_NO_MEM;
    }

    mdns_priv_service_unlock();
    return ESP_OK;
}

mdns_browse_record_t *mdns_browse_record_ptr_new(const char *service, const char *proto, const char *subtype,
                                                 mdns_browse_record_notify_t notifier)
{
    mdns_browse_record_t *browse_record = NULL;

    if (!mdns_priv_is_server_init() || !notifier || mdns_utils_str_null_or_empty(service)
            || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > 63) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return NULL;
    }

    browse_record = browse_record_init(service, proto, subtype, MDNS_BROWSE_RECORD_TYPE_PTR);
    if (!browse_record) {
        return NULL;
    }

    browse_record->notifier = notifier;

    mdns_priv_service_lock();

    for (mdns_browse_record_t *it = s_browse_record; it; it = it->next) {
        if (it->state != BROWSE_OFF && browse_records_match(it, browse_record)) {
            ESP_LOGW(TAG, "Record browse already exists: %s.%s %s", browse_record->service, browse_record->proto,
                     browse_record->subtype ? browse_record->subtype : "");
            goto error;
        }
    }

    if (send_browse_record_action(ACTION_BROWSE_RECORD_START, browse_record) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send record browse start action");
        goto error;
    }

    browse_record->next = s_browse_record;
    s_browse_record = browse_record;

    mdns_priv_service_unlock();
    return browse_record;

error:
    mdns_priv_service_unlock();
    browse_record_item_free(browse_record);
    return NULL;
}

esp_err_t mdns_browse_record_delete(mdns_browse_record_t *browse_record)
{
    bool found = false;
    esp_err_t err = ESP_OK;

    if (!browse_record) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!mdns_priv_is_server_init()) {
        return ESP_ERR_INVALID_STATE;
    }

    mdns_priv_service_lock();
    mdns_browse_state_t prev_state = BROWSE_OFF;
    for (mdns_browse_record_t *it = s_browse_record; it; it = it->next) {
        if (it->state != BROWSE_OFF && it == browse_record) {
            prev_state = it->state;
            browse_record->state = BROWSE_OFF;
            found = true;
            break;
        }
    }

    if (!found) {
        mdns_priv_service_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    err = send_browse_record_action(ACTION_BROWSE_RECORD_END, browse_record);
    if (err != ESP_OK) {
        browse_record->state = prev_state;
    }

    mdns_priv_service_unlock();
    return err;
}
