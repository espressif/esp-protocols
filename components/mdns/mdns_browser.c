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
#include "mdns_cache.h"

#define MDNS_CACHE_RECORD_BROWSE_SHARED (mdns_cache_record_mask_t) \
                                        (MDNS_CACHE_RECORD_SRV | MDNS_CACHE_RECORD_TXT | MDNS_CACHE_RECORD_ADDR)

static const char *TAG = "mdns_browser";

static mdns_browse_t *s_browse;

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
            mdns_results_free(result);
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

    mdns_results_free(result);
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
