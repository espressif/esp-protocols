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

static const char *TAG = "mdns_send";


/**
 * @brief  Send PTR query packet to all available interfaces for browsing.
 */
void _mdns_browse_send(mdns_browse_t *browse, mdns_if_t interface)
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

/**
 * @brief  Free a browse item (Not free the list).
 */
void _mdns_browse_item_free(mdns_browse_t *browse)
{
    mdns_mem_free(browse->service);
    mdns_mem_free(browse->proto);
    if (browse->result) {
        _mdns_query_results_free(browse->result);
    }
    mdns_mem_free(browse);
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
