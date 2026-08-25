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

    mdns_mem_free(browse->instance);
    mdns_mem_free(browse->service);
    mdns_mem_free(browse->proto);
    mdns_mem_free(browse->subtype);
    mdns_mem_free(browse);
}

/**
 * @brief   Send a query packet for browsing.
 */
static void browse_send(mdns_browse_t *browse, mdns_if_t interface, mdns_ip_protocol_t ip_protocol)
{
    if (!browse || browse->state != BROWSE_RUNNING) {
        return;
    }

    uint16_t record_type = 0;
    switch (browse->type) {
    case MDNS_BROWSE_TYPE_SERVICE:
    case MDNS_BROWSE_TYPE_RECORD_PTR:
        record_type = MDNS_TYPE_PTR;
        break;
    case MDNS_BROWSE_TYPE_RECORD_SRV:
        record_type = MDNS_TYPE_SRV;
        break;
    case MDNS_BROWSE_TYPE_RECORD_TXT:
        record_type = MDNS_TYPE_TXT;
        break;
    default:
        return;
    }

    mdns_search_once_t search = {
        .type = record_type,
        .unicast = record_type != MDNS_TYPE_PTR,
        .instance = browse->instance,
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
    case MDNS_BROWSE_TYPE_RECORD_SRV:
    case MDNS_BROWSE_TYPE_RECORD_TXT:
        return mdns_utils_str_case_equal(a->instance, b->instance)
               && mdns_utils_str_case_equal(a->service, b->service)
               && mdns_utils_str_case_equal(a->proto, b->proto);
    default:
        return false;
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
    case MDNS_BROWSE_TYPE_RECORD_SRV:
        return MDNS_CACHE_RECORD_SRV;
    case MDNS_BROWSE_TYPE_RECORD_TXT:
        return MDNS_CACHE_RECORD_TXT;
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

bool mdns_priv_browse_has_service(const char *instance, const char *service, const char *proto)
{
    for (const mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_RUNNING && browse_matches_identity(it, service, proto)
                && (it->type == MDNS_BROWSE_TYPE_SERVICE || it->type == MDNS_BROWSE_TYPE_RECORD_PTR
                    || mdns_utils_str_case_equal(it->instance, instance))) {
            return true;
        }
    }
    return false;
}

bool mdns_priv_browse_has_subtype(const char *service, const char *proto, const char *subtype)
{
    for (const mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_RUNNING && (it->type == MDNS_BROWSE_TYPE_SERVICE || it->type == MDNS_BROWSE_TYPE_RECORD_PTR)
                && browse_matches_identity_with_subtype(it, service, proto, subtype)) {
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
static mdns_browse_t *browse_init(const char *instance, const char *service, const char *proto, const char *subtype,
                                  mdns_browse_notify_t notifier, mdns_browse_type_t type)
{
    mdns_browse_t *browse = (mdns_browse_t *)mdns_mem_calloc(1, sizeof(mdns_browse_t));

    if (!browse) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    browse->type = type;
    browse->state = BROWSE_INIT;
    if (!mdns_utils_str_null_or_empty(instance)) {
        browse->instance = mdns_mem_strndup(instance, MDNS_NAME_BUF_LEN - 1);
        if (!browse->instance) {
            HOOK_MALLOC_FAILED;
            browse_item_free(browse);
            return NULL;
        }
    }
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
 * @brief Send initial queries for a registered browse.
 */
static void browse_start(mdns_browse_t *browse)
{
    if (!browse || browse->state != BROWSE_INIT) {
        return;
    }
    browse->state = BROWSE_RUNNING;

    if (mdns_priv_cache_notify_browse(browse) != ESP_OK) {
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
            if (it->type != MDNS_BROWSE_TYPE_SERVICE && it->type != MDNS_BROWSE_TYPE_RECORD_PTR) {
                break;
            }
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
            if (it->type == MDNS_BROWSE_TYPE_SERVICE || it->type == MDNS_BROWSE_TYPE_RECORD_PTR) {
                if (mdns_utils_str_case_equal(name->service, it->service) && mdns_utils_str_case_equal(name->proto, it->proto)) {
                    return it;
                }
            } else if ((type == MDNS_TYPE_SRV && it->type == MDNS_BROWSE_TYPE_RECORD_SRV)
                       || (type == MDNS_TYPE_TXT && it->type == MDNS_BROWSE_TYPE_RECORD_TXT)) {
                if (mdns_utils_str_case_equal(name->host, it->instance) && mdns_utils_str_case_equal(name->service, it->service)
                        && mdns_utils_str_case_equal(name->proto, it->proto)) {
                    return it;
                }
            }
            break;
        case MDNS_TYPE_A:
        case MDNS_TYPE_AAAA:
            if (mdns_priv_cache_host_has_service(name->host, mdns_priv_get_esp_netif(tcpip_if),
                                                 ip_protocol, it->instance, it->service, it->proto)) {
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
}

/**
 * @brief Check if a running browse `_service._proto` matches a service cache `_service._proto`.
 */
static bool browse_matches_service_cache(const mdns_browse_t *browse, const mdns_service_cache_t *service)
{
    if (!browse || !service || browse->state != BROWSE_RUNNING || !browse->notifier) {
        return false;
    }

    switch (browse->type) {
    case MDNS_BROWSE_TYPE_SERVICE:
    case MDNS_BROWSE_TYPE_RECORD_PTR:
        return mdns_utils_str_case_equal(browse->service, service->service)
               && mdns_utils_str_case_equal(browse->proto, service->proto);
    case MDNS_BROWSE_TYPE_RECORD_SRV:
        return service->srv_present
               && mdns_utils_str_case_equal(browse->service, service->service)
               && mdns_utils_str_case_equal(browse->proto, service->proto)
               && mdns_utils_str_case_equal(browse->instance, service->instance_name);
    case MDNS_BROWSE_TYPE_RECORD_TXT:
        return service->txt_present
               && mdns_utils_str_case_equal(browse->service, service->service)
               && mdns_utils_str_case_equal(browse->proto, service->proto)
               && mdns_utils_str_case_equal(browse->instance, service->instance_name);
    default:
        return false;
    }
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
    if (browse->type != MDNS_BROWSE_TYPE_SERVICE && browse->type != MDNS_BROWSE_TYPE_RECORD_PTR) {
        return true;
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
static esp_err_t browse_build_and_notify_temp_result(mdns_browse_t *browse, const mdns_cache_entry_t *entry,
                                                     const mdns_service_cache_t *service, const mdns_cache_subtype_t *subtype,
                                                     mdns_cache_record_mask_t removed_records)
{
    mdns_result_t *result = NULL;
    esp_err_t ret = mdns_priv_service_cache_to_result(entry, service, subtype, get_browse_projection_record_mask(browse),
                                                      removed_records, &result);
    if (ret != ESP_OK) {
        return ret;
    }

    DBG_BROWSE_RESULTS(result, browse);
    if (browse->notifier) {
        browse->notifier(result);
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
    case MDNS_BROWSE_TYPE_RECORD_SRV:
        return (records & MDNS_CACHE_RECORD_SRV) != 0;
    case MDNS_BROWSE_TYPE_RECORD_TXT:
        return (records & MDNS_CACHE_RECORD_TXT) != 0;
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
        updated &= (browse_build_and_notify_temp_result(browse, entry, service, subtype, MDNS_CACHE_RECORD_INVALID) == ESP_OK);
    }

    return updated;
}

esp_err_t mdns_priv_browse_notify_from_service_cache(const mdns_cache_entry_t *entry, const mdns_service_cache_t *service,
                                                     mdns_browse_t *browse)
{
    const mdns_cache_subtype_t *subtype = NULL;

    if (!entry || !service || !browse) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!browse_matches_service_cache_with_subtype(browse, service, &subtype)) {
        return ESP_ERR_NOT_FOUND;
    }

    return browse_build_and_notify_temp_result(browse, entry, service, subtype, MDNS_CACHE_RECORD_INVALID);
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
        notified &= (browse_build_and_notify_temp_result(browse, entry, service, subtype, browse_record_mask) == ESP_OK);
    }

    return notified;
}

static mdns_browse_t *browse_new(const char *instance, const char *service, const char *proto, const char *subtype, mdns_browse_notify_t notifier,
                                 mdns_browse_type_t type)
{
    mdns_browse_t *browse = NULL;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)
            || !notifier) {
        return NULL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > MDNS_SUBTYPE_MAX_LEN) {
        ESP_LOGE(TAG, "Subtype string length must not exceed %d bytes", MDNS_SUBTYPE_MAX_LEN);
        return NULL;
    }

    browse = browse_init(instance, service, proto, subtype, notifier, type);
    if (!browse) {
        return NULL;
    }

    mdns_priv_service_lock();
    // Check if the browse already exists before sending action
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_OFF && browse_match(it, browse)) {
            bool has_subtype = !mdns_utils_str_null_or_empty(subtype);
            ESP_LOGW(TAG, "Browse already exists: %s%s%s.%s", has_subtype ? subtype : "",
                     has_subtype ? "._sub." : "", browse->service, browse->proto);
            goto error;
        }
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

/**
 * @defgroup MDNS_PUBLIC_API
 */
mdns_browse_t *mdns_browse_new(const char *service, const char *proto, mdns_browse_notify_t notifier)
{
    return browse_new(NULL, service, proto, NULL, notifier, MDNS_BROWSE_TYPE_SERVICE);
}

mdns_browse_t *mdns_browse_new_with_subtype(const char *service, const char *proto, const char *subtype, mdns_browse_notify_t notifier)
{
    return browse_new(NULL, service, proto, subtype, notifier, MDNS_BROWSE_TYPE_SERVICE);
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
    if (subtype && strlen(subtype) > MDNS_SUBTYPE_MAX_LEN) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return ESP_FAIL;
    }

    mdns_priv_service_lock();
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_OFF && it->type == MDNS_BROWSE_TYPE_SERVICE
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

    if (send_browse_action(ACTION_BROWSE_END, browse) != ESP_OK) {
        browse->state = prev_state;
        mdns_priv_service_unlock();
        return ESP_ERR_NO_MEM;
    }

    mdns_priv_service_unlock();
    return ESP_OK;
}

mdns_browse_t *mdns_browse_record_ptr_new(const char *service, const char *proto, const char *subtype,
                                          mdns_browse_notify_t notifier)
{
    return browse_new(NULL, service, proto, subtype, notifier, MDNS_BROWSE_TYPE_RECORD_PTR);
}

mdns_browse_t *mdns_browse_record_srv_new(const char *instance, const char *service, const char *proto,
                                          mdns_browse_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(instance)) {
        return NULL;
    }
    return browse_new(instance, service, proto, NULL, notifier, MDNS_BROWSE_TYPE_RECORD_SRV);
}

mdns_browse_t *mdns_browse_record_txt_new(const char *instance, const char *service, const char *proto,
                                          mdns_browse_notify_t notifier)
{
    if (mdns_utils_str_null_or_empty(instance)) {
        return NULL;
    }
    return browse_new(instance, service, proto, NULL, notifier, MDNS_BROWSE_TYPE_RECORD_TXT);
}

esp_err_t mdns_browse_delete_handle(mdns_browse_t *browse)
{
    bool found = false;
    esp_err_t err = ESP_OK;

    if (!browse) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!mdns_priv_is_server_init()) {
        return ESP_ERR_INVALID_STATE;
    }

    mdns_priv_service_lock();
    mdns_browse_state_t prev_state = BROWSE_OFF;
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state != BROWSE_OFF && it == browse) {
            prev_state = it->state;
            browse->state = BROWSE_OFF;
            found = true;
            break;
        }
    }

    if (!found) {
        mdns_priv_service_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    err = send_browse_action(ACTION_BROWSE_END, browse);
    if (err != ESP_OK) {
        browse->state = prev_state;
    }

    mdns_priv_service_unlock();
    return err;
}
