/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "mdns_private.h"
#include "mdns_browse.h"
#include "mdns_send.h"
#include "mdns_mem_caps.h"
#include "mdns_debug.h"
#include "mdns_utils.h"
#include "mdns_querier.h"
#include "esp_log.h"

static const char *TAG = "mdns_browser";

static mdns_browse_t *s_browse;

/**
 * @brief  Browse action
 */
static esp_err_t _mdns_send_browse_action(mdns_action_type_t type, mdns_browse_t *browse)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));

    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.browse_add.browse = browse;
    if (!mdns_action_queue(action)) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief  Free a browse item (Not free the list).
 */
static void _mdns_browse_item_free(mdns_browse_t *browse)
{
    mdns_mem_free(browse->service);
    mdns_mem_free(browse->proto);
    if (browse->result) {
        _mdns_query_results_free(browse->result);
    }
    mdns_mem_free(browse);
}

static void _mdns_browse_sync(mdns_browse_sync_t *browse_sync)
{
    mdns_browse_t *browse = browse_sync->browse;
    mdns_browse_result_sync_t *sync_result = browse_sync->sync_result;
    while (sync_result) {
        mdns_result_t *result = sync_result->result;
        DBG_BROWSE_RESULTS(result, browse_sync->browse);
        browse->notifier(result);
        if (result->ttl == 0) {
            queueDetach(mdns_result_t, browse->result, result);
            // Just free current result
            result->next = NULL;
            mdns_query_results_free(result);
        }
        sync_result = sync_result->next;
    }
}

/**
 * @brief  Send PTR query packet to all available interfaces for browsing.
 */
static void _mdns_browse_send(mdns_browse_t *browse, mdns_if_t interface)
{
    // Using search once for sending the PTR query
    mdns_search_once_t search = {0};

    search.instance = NULL;
    search.service = browse->service;
    search.proto = browse->proto;
    search.type = MDNS_TYPE_PTR;
    search.unicast = false;
    search.result = NULL;
    search.next = NULL;

    for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
        _mdns_search_send_pcb(&search, interface, (mdns_ip_protocol_t)protocol_idx);
    }
}

void mdns_browse_send_all(mdns_if_t mdns_if)
{
    mdns_browse_t *browse = s_browse;
    while (browse) {
        _mdns_browse_send(browse, mdns_if);
        browse = browse->next;
    }
}

void mdns_browse_free(void)
{
    while (s_browse) {
        mdns_browse_t *b = s_browse;
        s_browse = s_browse->next;
        _mdns_browse_item_free(b);
    }
}

/**
 * @brief  Mark browse as finished, remove and free it from browse chain
 */
static void _mdns_browse_finish(mdns_browse_t *browse)
{
    browse->state = BROWSE_OFF;
    mdns_browse_t *b = s_browse;
    mdns_browse_t *target_free = NULL;
    while (b) {
        if (strlen(b->service) == strlen(browse->service) && memcmp(b->service, browse->service, strlen(b->service)) == 0 &&
                strlen(b->proto) == strlen(browse->proto) && memcmp(b->proto, browse->proto, strlen(b->proto)) == 0) {
            target_free = b;
            b = b->next;
            queueDetach(mdns_browse_t, s_browse, target_free);
            _mdns_browse_item_free(target_free);
        } else {
            b = b->next;
        }
    }
    _mdns_browse_item_free(browse);
}

/**
 * @brief  Allocate new browse structure
 */
static mdns_browse_t *_mdns_browse_init(const char *service, const char *proto, mdns_browse_notify_t notifier)
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
            _mdns_browse_item_free(browse);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(proto)) {
        browse->proto = mdns_mem_strndup(proto, MDNS_NAME_BUF_LEN - 1);
        if (!browse->proto) {
            _mdns_browse_item_free(browse);
            return NULL;
        }
    }

    browse->notifier = notifier;
    return browse;
}

mdns_browse_t *mdns_browse_new(const char *service, const char *proto, mdns_browse_notify_t notifier)
{
    mdns_browse_t *browse = NULL;

    if (is_mdns_server() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }

    browse = _mdns_browse_init(service, proto, notifier);
    if (!browse) {
        return NULL;
    }

    if (_mdns_send_browse_action(ACTION_BROWSE_ADD, browse)) {
        _mdns_browse_item_free(browse);
        return NULL;
    }

    return browse;
}

esp_err_t mdns_browse_delete(const char *service, const char *proto)
{
    mdns_browse_t *browse = NULL;

    if (!is_mdns_server() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_FAIL;
    }

    browse = _mdns_browse_init(service, proto, NULL);
    if (!browse) {
        return ESP_ERR_NO_MEM;
    }

    if (_mdns_send_browse_action(ACTION_BROWSE_END, browse)) {
        _mdns_browse_item_free(browse);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief  Add new browse to the browse chain
 */
static void _mdns_browse_add(mdns_browse_t *browse)
{
    browse->state = BROWSE_RUNNING;
    mdns_browse_t *queue = s_browse;
    bool found = false;
    // looking for this browse in active browses
    while (queue) {
        if (strlen(queue->service) == strlen(browse->service) && memcmp(queue->service, browse->service, strlen(queue->service)) == 0 &&
                strlen(queue->proto) == strlen(browse->proto) && memcmp(queue->proto, browse->proto, strlen(queue->proto)) == 0) {
            found = true;
            break;
        }
        queue = queue->next;
    }
    if (!found) {
        browse->next = s_browse;
        s_browse = browse;
    }
    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        _mdns_browse_send(browse, (mdns_if_t)interface_idx);
    }
    if (found) {
        _mdns_browse_item_free(browse);
    }
}

/**
 * @brief  Called from packet parser to find matching running search
 */
mdns_browse_t *_mdns_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_browse_t *b = s_browse;
    // For browse, we only care about the SRV, TXT, A and AAAA
    if (type != MDNS_TYPE_SRV && type != MDNS_TYPE_A && type != MDNS_TYPE_AAAA && type != MDNS_TYPE_TXT) {
        return NULL;
    }
    mdns_result_t *r = NULL;
    while (b) {
        if (type == MDNS_TYPE_SRV || type == MDNS_TYPE_TXT) {
            if (strcasecmp(name->service, b->service)
                    || strcasecmp(name->proto, b->proto)) {
                b = b->next;
                continue;
            }
            return b;
        } else if (type == MDNS_TYPE_A || type == MDNS_TYPE_AAAA) {
            r = b->result;
            while (r) {
                if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(name->host, r->hostname)) {
                    return b;
                }
                r = r->next;
            }
            b = b->next;
            continue;
        }
    }
    return b;
}

static void _mdns_sync_browse_result_link_free(mdns_browse_sync_t *browse_sync)
{
    mdns_browse_result_sync_t *current = browse_sync->sync_result;
    mdns_browse_result_sync_t *need_free;
    while (current) {
        need_free = current;
        current = current->next;
        mdns_mem_free(need_free);
    }
    mdns_mem_free(browse_sync);
}

void mdns_browse_action(mdns_action_t *action, mdns_action_subtype_t type)
{
    if (type == ACTION_RUN) {
        switch (action->type) {
        case ACTION_BROWSE_ADD:
            _mdns_browse_add(action->data.browse_add.browse);
            break;
        case ACTION_BROWSE_SYNC:
            _mdns_browse_sync(action->data.browse_sync.browse_sync);
            _mdns_sync_browse_result_link_free(action->data.browse_sync.browse_sync);
            break;
        case ACTION_BROWSE_END:
            _mdns_browse_finish(action->data.browse_add.browse);
            break;
        default:
            abort();
        }
        return;
    }
    if (type == ACTION_CLEANUP) {
        switch (action->type) {
        case ACTION_BROWSE_ADD:
        //fallthrough
        case ACTION_BROWSE_END:
            _mdns_browse_item_free(action->data.browse_add.browse);
            break;
        case ACTION_BROWSE_SYNC:
            _mdns_sync_browse_result_link_free(action->data.browse_sync.browse_sync);
            break;
        default:
            abort();
        }
        return;
    }

}
