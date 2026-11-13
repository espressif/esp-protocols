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
#define MDNS_SUBTYPE_MAX_LEN 63

static const char *TAG = "mdns_browser";

static mdns_browse_t *s_browse;

/**
 * @brief  Browse action
 */
static esp_err_t send_browse_action(mdns_action_type_t type, mdns_browse_t *browse, mdns_browse_notifier_t *notifier)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));

    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.browse_add.browse = browse;
    action->data.browse_add.notifier = notifier;
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

    while (browse->notifiers) {
        mdns_browse_notifier_t *notifier = browse->notifiers;
        browse->notifiers = notifier->next;
        mdns_mem_free(notifier);
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
    if (!browse || browse->state != BROWSE_RUNNING) {
        return;
    }

    // Using search once for sending the PTR query
    mdns_search_once_t search = {
        .type = MDNS_TYPE_PTR,
        .unicast = false,
        .service = browse->service,
        .proto = browse->proto,
        .subtype = browse->subtype,
    };

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
 * @brief Check if two browses are identical.
 */
static bool browse_match(const mdns_browse_t *a, const mdns_browse_t *b)
{
    if (!a || !b || a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case MDNS_BROWSE_TYPE_SERVICE:
    case MDNS_BROWSE_TYPE_RECORD_PTR:
        return mdns_utils_str_case_equal(a->service, b->service)
               && mdns_utils_str_case_equal(a->proto, b->proto)
               && mdns_utils_str_case_equal_nullable(a->subtype, b->subtype);
    default:
        return false;
    }
}

/*
 * @brief Check if two `mdns_browse_notifier_t` items are identical.
 */
static bool browse_notifier_matches(const mdns_browse_notifier_t *a, const mdns_browse_notifier_t *b)
{
    if (!a || !b || a->type != b->type || a->context != b->context) {
        return false;
    }

    switch (a->type) {
    case MDNS_BROWSE_CALLBACK_LEGACY:
        return a->callback.legacy == b->callback.legacy;
    case MDNS_BROWSE_CALLBACK_WITH_CONTEXT:
        return a->callback.handler == b->callback.handler;
    default:
        return false;
    }
}

/*
 * @brief Invokes the callback of an `mdns_browse_notifier_t` item to notify the result
 */
static void browse_callback(const mdns_browse_notifier_t *notifier, mdns_result_t *result)
{
    switch (notifier->type) {
    case MDNS_BROWSE_CALLBACK_LEGACY:
        notifier->callback.legacy(result);
        break;
    case MDNS_BROWSE_CALLBACK_WITH_CONTEXT:
        notifier->callback.handler(result, notifier->context);
        break;
    }
}

/**
 * @brief Generate `mdns_cache_record_mask_t` from a browse item for removed records
 */
static mdns_cache_record_mask_t get_browse_record_mask(const mdns_browse_t *browse)
{
    if (!browse) {
        return MDNS_CACHE_RECORD_INVALID;
    }

    switch (browse->type) {
    case MDNS_BROWSE_TYPE_SERVICE:
    case MDNS_BROWSE_TYPE_RECORD_PTR:
        return mdns_utils_str_null_or_empty(browse->subtype) ? MDNS_CACHE_RECORD_PTR : MDNS_CACHE_RECORD_SUBTYPE;
    default:
        return MDNS_CACHE_RECORD_INVALID;
    }
}

/**
 * @brief Generate `mdns_cache_record_mask_t` from a browse item for `mdns_priv_service_cache_to_result()`
 */
static mdns_cache_record_mask_t get_browse_projection_record_mask(const mdns_browse_t *browse)
{
    mdns_cache_record_mask_t record_mask = get_browse_record_mask(browse);
    if (browse && browse->type == MDNS_BROWSE_TYPE_SERVICE) {
        record_mask |= MDNS_CACHE_RECORD_BROWSE_SHARED;
    }
    return record_mask;
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
 * @brief  Remove and free the notifier item from the browse item when `notifier` is not NULL,
 *         otherwise remove and free the browse from browse linked list.
 *         Browse will be freed when no notifiers exist.
 */
static void browse_finish(mdns_browse_t *browse, mdns_browse_notifier_t *notifier)
{
    if (!browse) {
        return;
    }

    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it == browse) {
            if (notifier) {
                queueDetach(mdns_browse_notifier_t, it->notifiers, notifier);
                mdns_mem_free(notifier);
            }

            if (!notifier || !browse->notifiers) {
                queueDetach(mdns_browse_t, s_browse, it);
                browse_item_free(it);
                mdns_priv_cache_trim_unsubscribed_records(esp_timer_get_time());
            }
            return;
        }
    }
}

/**
 * @brief  Allocate new browse structure
 */
static mdns_browse_t *browse_init(const char *service, const char *proto, const char *subtype,
                                  const mdns_browse_notifier_t *notifier, mdns_browse_type_t type)
{
    mdns_browse_t *browse = (mdns_browse_t *)mdns_mem_calloc(1, sizeof(mdns_browse_t));

    if (!browse) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    browse->type = type;
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

    browse->notifiers = mdns_mem_calloc(1, sizeof(mdns_browse_notifier_t));
    mdns_browse_notifier_t *node = browse->notifiers;
    if (!node) {
        HOOK_MALLOC_FAILED;
        browse_item_free(browse);
        return NULL;
    }
    node->type = notifier->type;
    node->callback = notifier->callback;
    node->context = notifier->context;
    node->active = true;

    return browse;
}

/**
 * @brief Replay cached results and send PTR queries after registering a callback.
 */
static void browse_start(mdns_browse_t *browse, mdns_browse_notifier_t *notifier)
{
    if (!browse || browse->state == BROWSE_OFF || !notifier || !notifier->active) {
        return;
    }
    browse->state = BROWSE_RUNNING;

    if (mdns_priv_cache_notify_browse(browse, notifier) != ESP_OK) {
        ESP_LOGW(TAG, "Fails to notify browse on registration");
    }

    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
            browse_send(browse, (mdns_if_t) interface_idx, (mdns_ip_protocol_t) protocol_idx);
        }
    }
}

mdns_browse_t *mdns_priv_browse_find_consumer(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_RUNNING) {
            continue;
        }
        // For browse, we only care about PTR, SRV, TXT, A and AAAA
        switch (type) {
        case MDNS_TYPE_PTR: {
            bool subtype_matches = name->sub ? mdns_utils_str_case_equal(name->host, it->subtype) :
                                   mdns_utils_str_null_or_empty(it->subtype);
            if (mdns_utils_str_case_equal(name->service, it->service) && mdns_utils_str_case_equal(name->proto, it->proto)
                    && subtype_matches) {
                return it;
            }
            break;
        }
        case MDNS_TYPE_SRV:
        case MDNS_TYPE_TXT:
            if (mdns_utils_str_case_equal(name->service, it->service) && mdns_utils_str_case_equal(name->proto, it->proto)) {
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
        default:
            return NULL;
        }
    }
    return NULL;
}

void mdns_priv_browse_action(mdns_action_t *action, mdns_action_subtype_t type)
{
    if (type == ACTION_RUN) {
        switch (action->type) {
        case ACTION_BROWSE_START:
            browse_start(action->data.browse_add.browse, action->data.browse_add.notifier);
            break;
        case ACTION_BROWSE_END:
            browse_finish(action->data.browse_add.browse, action->data.browse_add.notifier);
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
    return browse && service && browse->state == BROWSE_RUNNING && browse->notifiers
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
 *
 * @note Notify the specific callback when `notifier` is not NULL, otherwise notify all active callbacks under `browse`
 */
static esp_err_t browse_build_and_notify_temp_result(mdns_browse_t *browse, const mdns_cache_entry_t *entry,
                                                     const mdns_service_cache_t *service, const mdns_cache_subtype_t *subtype,
                                                     mdns_cache_record_mask_t removed_records, const mdns_browse_notifier_t *notifier)
{
    mdns_result_t *result = NULL;
    esp_err_t ret = mdns_priv_service_cache_to_result(entry, service, subtype, get_browse_projection_record_mask(browse),
                                                      removed_records, &result);
    if (ret != ESP_OK) {
        return ret;
    }

    DBG_BROWSE_RESULTS(result, browse);

    if (notifier) {
        browse_callback(notifier, result);
    } else {
        for (mdns_browse_notifier_t *it = browse->notifiers; it; it = it->next) {
            if (it->active) {
                browse_callback(it, result);
            }
        }
    }

    mdns_priv_query_results_free(result);
    return ESP_OK;
}

static bool browse_need_update(const mdns_browse_t *browse, const mdns_service_cache_t *service,
                               const mdns_cache_subtype_t *subtype, mdns_cache_record_mask_t records)
{
    if (!browse || !service) {
        return false;
    }

    bool has_subtype = !mdns_utils_str_null_or_empty(browse->subtype);
    bool ptr_need_update = has_subtype ? (records & MDNS_CACHE_RECORD_SUBTYPE) && subtype && subtype->pending_sync
                           : (records & MDNS_CACHE_RECORD_PTR);

    switch (browse->type) {
    case MDNS_BROWSE_TYPE_SERVICE:
        return (records & MDNS_CACHE_RECORD_BROWSE_SHARED) || ptr_need_update;
    case MDNS_BROWSE_TYPE_RECORD_PTR:
        return ptr_need_update;
    default:
        return false;
    }
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
        updated &= (browse_build_and_notify_temp_result(browse, entry, service, subtype, MDNS_CACHE_RECORD_INVALID, NULL) == ESP_OK);
    }

    return updated;
}

esp_err_t mdns_priv_browse_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                     mdns_browse_t *browse, const mdns_browse_notifier_t *notifier)
{
    const mdns_cache_subtype_t *subtype = NULL;

    if (!entry || !service || !browse || !notifier) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!browse_matches_service_cache_with_subtype(browse, service, &subtype)) {
        return ESP_ERR_NOT_FOUND;
    }

    return browse_build_and_notify_temp_result(browse, entry, service, subtype, MDNS_CACHE_RECORD_INVALID, notifier);
}

bool mdns_priv_browse_notify_record_removed_from_service_cache(const mdns_cache_entry_t *entry,
                                                               const mdns_service_cache_t *service,
                                                               const mdns_cache_subtype_t *subtype,
                                                               mdns_cache_record_type_t removed_record)
{
    if (!entry || !service) {
        return false;
    }

    bool notified = true;

    for (mdns_browse_t *browse = s_browse; browse; browse = browse->next) {
        mdns_cache_record_mask_t browse_record_mask = get_browse_record_mask(browse);
        if (!(browse_record_mask & removed_record)) {
            continue;
        }
        if (!browse_matches_service_cache(browse, service)) {
            continue;
        }
        if (subtype && !mdns_utils_str_case_equal(subtype->subtype, browse->subtype)) {
            continue;
        }
        notified &= (browse_build_and_notify_temp_result(browse, entry, service, subtype, browse_record_mask, NULL) == ESP_OK);
    }

    return notified;
}

static mdns_browse_t *browse_new(const char *service, const char *proto, const char *subtype,
                                 const mdns_browse_notifier_t *notifier, mdns_browse_type_t type)
{
    mdns_browse_t *browse = NULL;
    mdns_browse_notifier_t *notifier_item = NULL;
    bool browse_exists = false;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)
            || !notifier) {
        return NULL;
    }
    switch (notifier->type) {
    case MDNS_BROWSE_CALLBACK_LEGACY:
        if (!notifier->callback.legacy) {
            return NULL;
        }
        break;
    case MDNS_BROWSE_CALLBACK_WITH_CONTEXT:
        if (!notifier->callback.handler) {
            return NULL;
        }
        break;
    default:
        return NULL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > MDNS_SUBTYPE_MAX_LEN) {
        ESP_LOGE(TAG, "Subtype string length must not exceed %d bytes", MDNS_SUBTYPE_MAX_LEN);
        return NULL;
    }

    browse = browse_init(service, proto, subtype, notifier, type);
    if (!browse) {
        return NULL;
    }
    notifier_item = browse->notifiers;
    notifier_item->active = true;

    mdns_priv_service_lock();
    // Check if the browse already exists before sending action
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_OFF && browse_match(it, browse)) {
            for (mdns_browse_notifier_t *node = it->notifiers; node; node = node->next) {
                if (node->active && browse_notifier_matches(node, notifier_item)) {
                    bool has_subtype = !mdns_utils_str_null_or_empty(subtype);
                    ESP_LOGW(TAG, "Browse already exists: %s%s%s.%s", has_subtype ? subtype : "",
                             has_subtype ? "._sub." : "", browse->service, browse->proto);
                    goto error;
                }
            }
            browse_exists = true;
            // `browse->notifiers` will be reused, other components are freed.
            browse->notifiers = NULL;
            browse_item_free(browse);
            browse = it;
        }
    }

    if (send_browse_action(ACTION_BROWSE_START, browse, notifier_item) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send browse start action");
        goto error;
    }

    if (!browse_exists) {
        browse->next = s_browse;
        s_browse = browse;
    } else {
        // Reuse the `mdns_browse_notifier_t` created by `browse_init()`
        notifier_item->next = browse->notifiers;
        browse->notifiers = notifier_item;
    }
    mdns_priv_service_unlock();
    return browse;

error:
    if (!browse_exists) {
        browse_item_free(browse);
    } else {
        mdns_mem_free(notifier_item);
    }
    mdns_priv_service_unlock();
    return NULL;
}

static esp_err_t browse_delete(const char *service, const char *proto, const char *subtype, mdns_browse_type_t type)
{
    mdns_browse_t *browse = NULL;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_FAIL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > MDNS_SUBTYPE_MAX_LEN) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return ESP_FAIL;
    }

    mdns_priv_service_lock();
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_OFF && it->type == type
                && browse_matches_identity_with_subtype(it, service, proto, subtype)) {
            browse = it;
            break;
        }
    }
    if (!browse) {
        mdns_priv_service_unlock();
        return ESP_FAIL;
    }

    mdns_browse_state_t prev_state = browse->state;
    browse->state = BROWSE_OFF;

    if (send_browse_action(ACTION_BROWSE_END, browse, NULL) != ESP_OK) {
        browse->state = prev_state;
        mdns_priv_service_unlock();
        return ESP_ERR_NO_MEM;
    }

    mdns_priv_service_unlock();
    return ESP_OK;
}

static esp_err_t browse_delete_notifier(const char *service, const char *proto, const char *subtype,
                                        mdns_browse_type_t type, const mdns_browse_notifier_t *notifier)
{
    if (mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto) || !notifier) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (notifier->type) {
    case MDNS_BROWSE_CALLBACK_LEGACY:
        if (!notifier->callback.legacy) {
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case MDNS_BROWSE_CALLBACK_WITH_CONTEXT:
        if (!notifier->callback.handler) {
            return ESP_ERR_INVALID_ARG;
        }
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    if (subtype && strlen(subtype) > MDNS_SUBTYPE_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mdns_priv_is_server_init()) {
        return ESP_ERR_INVALID_STATE;
    }

    mdns_priv_service_lock();
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_OFF || it->type != type
                || !browse_matches_identity_with_subtype(it, service, proto, subtype)) {
            continue;
        }

        for (mdns_browse_notifier_t *node = it->notifiers; node; node = node->next) {
            if (!node->active || !browse_notifier_matches(node, notifier)) {
                continue;
            }

            mdns_browse_state_t prev_state = it->state;
            bool prev_notifier_state = node->active;

            node->active = false;
            // Stop the browse when no active callbacks remain
            bool has_active = false;
            for (mdns_browse_notifier_t *n = it->notifiers; n; n = n->next) {
                has_active |= n->active;
            }
            if (!has_active) {
                it->state = BROWSE_OFF;
            }

            esp_err_t err = send_browse_action(ACTION_BROWSE_END, it, node);
            if (err != ESP_OK) {
                it->state = prev_state;
                node->active = prev_notifier_state;
            }
            mdns_priv_service_unlock();
            return err;
        }
        break;
    }
    mdns_priv_service_unlock();
    return ESP_ERR_NOT_FOUND;
}

/**
 * @defgroup MDNS_PUBLIC_API
 */
mdns_browse_t *mdns_browse_new(const char *service, const char *proto, mdns_browse_notify_t notifier)
{
    return mdns_browse_new_with_subtype(service, proto, NULL, notifier);
}

mdns_browse_t *mdns_browse_new_with_context(const char *service, const char *proto, mdns_browse_handler_t handler,
                                            void *context)
{
    return mdns_browse_new_with_subtype_with_context(service, proto, NULL, handler, context);
}

mdns_browse_t *mdns_browse_new_with_subtype(const char *service, const char *proto, const char *subtype, mdns_browse_notify_t notifier)
{
    const mdns_browse_notifier_t spec = {
        .type = MDNS_BROWSE_CALLBACK_LEGACY,
        .callback.legacy = notifier,
    };
    return browse_new(service, proto, subtype, &spec, MDNS_BROWSE_TYPE_SERVICE);
}

mdns_browse_t *mdns_browse_new_with_subtype_with_context(const char *service, const char *proto, const char *subtype,
                                                         mdns_browse_handler_t handler, void *context)
{
    const mdns_browse_notifier_t spec = {
        .type = MDNS_BROWSE_CALLBACK_WITH_CONTEXT,
        .callback.handler = handler,
        .context = context,
    };
    return browse_new(service, proto, subtype, &spec, MDNS_BROWSE_TYPE_SERVICE);
}

esp_err_t mdns_browse_delete(const char *service, const char *proto)
{
    return mdns_browse_delete_with_subtype(service, proto, NULL);
}

esp_err_t mdns_browse_delete_with_subtype(const char *service, const char *proto, const char *subtype)
{
    return browse_delete(service, proto, subtype, MDNS_BROWSE_TYPE_SERVICE);
}

mdns_browse_t *mdns_browse_record_ptr_new(const char *service, const char *proto, const char *subtype,
                                          mdns_browse_handler_t handler, void *context)
{
    const mdns_browse_notifier_t spec = {
        .type = MDNS_BROWSE_CALLBACK_WITH_CONTEXT,
        .callback.handler = handler,
        .context = context,
    };
    return browse_new(service, proto, subtype, &spec, MDNS_BROWSE_TYPE_RECORD_PTR);
}

esp_err_t mdns_browse_record_ptr_delete(const char *service, const char *proto, const char *subtype)
{
    return browse_delete(service, proto, subtype, MDNS_BROWSE_TYPE_RECORD_PTR);
}

esp_err_t mdns_browse_delete_notifier(const char *service, const char *proto, mdns_browse_notify_t notifier)
{
    return mdns_browse_delete_notifier_with_subtype(service, proto, NULL, notifier);
}

esp_err_t mdns_browse_delete_notifier_with_subtype(const char *service, const char *proto, const char *subtype,
                                                   mdns_browse_notify_t notifier)
{
    const mdns_browse_notifier_t spec = {
        .type = MDNS_BROWSE_CALLBACK_LEGACY,
        .callback.legacy = notifier,
    };
    return browse_delete_notifier(service, proto, subtype, MDNS_BROWSE_TYPE_SERVICE, &spec);
}

esp_err_t mdns_browse_delete_handler_with_context(const char *service, const char *proto,
                                                  mdns_browse_handler_t handler, void *context)
{
    return mdns_browse_delete_handler_with_subtype_with_context(service, proto, NULL, handler, context);
}

esp_err_t mdns_browse_delete_handler_with_subtype_with_context(const char *service, const char *proto,
                                                               const char *subtype, mdns_browse_handler_t handler,
                                                               void *context)
{
    const mdns_browse_notifier_t spec = {
        .type = MDNS_BROWSE_CALLBACK_WITH_CONTEXT,
        .callback.handler = handler,
        .context = context,
    };
    return browse_delete_notifier(service, proto, subtype, MDNS_BROWSE_TYPE_SERVICE, &spec);
}

esp_err_t mdns_browse_record_ptr_delete_handler(const char *service, const char *proto, const char *subtype,
                                                mdns_browse_handler_t handler, void *context)
{
    const mdns_browse_notifier_t spec = {
        .type = MDNS_BROWSE_CALLBACK_WITH_CONTEXT,
        .callback.handler = handler,
        .context = context,
    };
    return browse_delete_notifier(service, proto, subtype, MDNS_BROWSE_TYPE_RECORD_PTR, &spec);
}
