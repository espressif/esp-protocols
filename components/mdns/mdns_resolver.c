/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "mdns_mem_caps.h"
#include "mdns_querier.h"
#include "mdns_resolver.h"
#include "mdns_responder.h"
#include "mdns_service.h"
#include "mdns_utils.h"

static const char *TAG = "mdns_resolver";

static mdns_resolver_t *s_resolver;

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
    default:
        return false;
    }
}


static void resolver_item_free(mdns_resolver_t *resolver)
{
    if (!resolver) {
        return;
    }

    mdns_mem_free(resolver->service);
    mdns_mem_free(resolver->proto);
    mdns_mem_free(resolver->subtype);
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

static void resolver_send_question(char *service, char *proto, char *subtype, uint16_t record_type,
                                   mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_search_once_t search = {
        .type = record_type,
        .unicast = record_type != MDNS_TYPE_PTR,
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
    default:
        ESP_LOGE(TAG, "Invalid resolver type: %d", resolver->type);
        return;
    }

    resolver_send_question(resolver->service, resolver->proto, resolver->subtype, record_type, mdns_if, ip_protocol);
}

static mdns_resolver_t *resolver_init(const char *service, const char *proto, const char *subtype,
                                      mdns_resolver_type_t type)
{
    mdns_resolver_t *resolver = (mdns_resolver_t *)mdns_mem_calloc(1, sizeof(mdns_resolver_t));
    if (!resolver) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    resolver->type = type;
    resolver->state = RESOLVER_INIT;

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

    return resolver;
}

static void resolver_start(mdns_resolver_t *resolver)
{
    if (!resolver || resolver->state != RESOLVER_INIT) {
        return;
    }
    resolver->state = RESOLVER_RUNNING;

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

/**
 * @defgroup MDNS_PUBCLIC_API
 */
mdns_resolver_t *mdns_ptr_resolver_new(const char *service, const char *proto, const char *subtype,
                                       mdns_ptr_resolver_notify_t notifier)
{
    mdns_resolver_t *resolver = NULL;

    if (!mdns_priv_is_server_init() || !notifier || mdns_utils_str_null_or_empty(service)
            || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }

    // Subtype identifier length limit defined in RFC 6763 Section 7.2
    if (subtype && strlen(subtype) > 63) {
        ESP_LOGE(TAG, "Subtype string length must not exceed 63 bytes");
        return NULL;
    }

    resolver = resolver_init(service, proto, subtype, MDNS_RESOLVER_TYPE_PTR);
    if (!resolver) {
        return NULL;
    }

    resolver->notifier.ptr = notifier;

    mdns_priv_service_lock();

    for (mdns_resolver_t *it = s_resolver; it; it = it->next) {
        if (it->state != RESOLVER_OFF && resolvers_match(it, resolver)) {
            ESP_LOGW(TAG, "Resolver already exists: %s.%s %s", resolver->service, resolver->proto,
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
