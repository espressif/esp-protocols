/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "sdkconfig.h"
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
    mdns_mem_free(browse->service);
    mdns_mem_free(browse->proto);
    if (browse->result) {
        mdns_priv_query_results_free(browse->result);
    }
    mdns_mem_free(browse);
}

/**
 * @brief Check that a browse is still linked in @c s_browse
 *
 * Sync batches borrow @c browse_sync->browse. ACTION_BROWSE_END may detach and
 * free that browse while a later ACTION_BROWSE_SYNC is still queued, so the
 * sync handler must not touch the pointer without checking.
 */
static bool browse_is_in_list(const mdns_browse_t *browse)
{
    for (const mdns_browse_t *b = s_browse; b != NULL; b = b->next) {
        if (b == browse) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check that a result node is still linked in the browse cache
 *
 * Sync entries hold a borrowed pointer to a node in @c browse->result. One node
 * can be referenced by several queued sync batches, and the first batch to see
 * it with @c ttl==0 detaches and frees it, so a later batch must not use its
 * pointer without checking.
 */
static bool result_is_cached(const mdns_browse_t *browse, const mdns_result_t *result)
{
    for (const mdns_result_t *r = browse->result; r != NULL; r = r->next) {
        if (r == result) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Deliver browse updates to the user notifier
 *
 * Invokes the notifier once per changed result accumulated for the current
 * packet. The passed @c result pointer is a live node in @c browse->result;
 * @c result->next is not cleared before the callback (only after TTL=0 removal).
 * See @ref mdns_browse_notify_t for how callers should use @c next.
 */
static void browse_sync(mdns_browse_sync_t *browse_sync)
{
    mdns_browse_t *browse = browse_sync->browse;
    // END may have already detached+freed this browse, or delete marked it off
    if (!browse_is_in_list(browse) || browse->state != BROWSE_RUNNING) {
        return;
    }
    mdns_browse_result_sync_t *sync_result = browse_sync->sync_result;
    while (sync_result) {
        mdns_result_t *result = sync_result->result;
        if (!result_is_cached(browse, result)) {
            // Already removed and freed by an earlier sync batch
            sync_result = sync_result->next;
            continue;
        }
        DBG_BROWSE_RESULTS(result, browse_sync->browse);
        browse->notifier(result);
        if (result->ttl == 0) {
            queueDetach(mdns_result_t, browse->result, result);
            // Just free current result
            result->next = NULL;
            mdns_priv_query_results_free(result);
        }
        sync_result = sync_result->next;
    }
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

void mdns_priv_browse_send_all(mdns_if_t mdns_if)
{
    for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
        mdns_priv_browse_send_by_ip_protocol(mdns_if, (mdns_ip_protocol_t) protocol_idx);
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
            return;
        }
    }
}

/**
 * @brief  Allocate new browse structure
 */
static mdns_browse_t *browse_init(const char *service, const char *proto, mdns_browse_notify_t notifier)
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

    browse->notifier = notifier;
    return browse;
}

/**
 * @brief  Send initial PTR queries for a registered browse.
 */
static void browse_start(mdns_browse_t *browse)
{
    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        for (uint8_t protocol_idx = 0; protocol_idx < MDNS_IP_PROTOCOL_MAX; protocol_idx++) {
            browse_send(browse, (mdns_if_t) interface_idx, (mdns_ip_protocol_t) protocol_idx);
        }
    }
}

static esp_err_t add_browse_result(mdns_browse_sync_t *sync_browse, mdns_result_t *r);

/**
 * @brief  Called from packet parser to find matching running search
 *
 * @note Called from the mDNS service task while the service lock is held.
 *       The returned browse is an active cache node that the parser may update.
 */
mdns_browse_t *mdns_priv_browse_find_ptr(mdns_name_t *name)
{
    mdns_browse_t *b = s_browse;

    if (mdns_utils_str_null_or_empty(name->service) || mdns_utils_str_null_or_empty(name->proto)) {
        return NULL;
    }

    while (b) {
        if (b->state == BROWSE_RUNNING && !strcasecmp(name->service, b->service) && !strcasecmp(name->proto, b->proto)) {
            return b;
        }
        b = b->next;
    }
    return NULL;
}

/**
 * @note Only one browse sync object is kept per parsed packet.  If @p sync
 *       is already allocated for a *different* browse, this function returns
 *       the existing object unchanged — callers that compare
 *       out_sync_browse->browse against the current browse will silently
 *       skip the update.  This is acceptable because mDNS answers for
 *       multiple browsed service types in a single packet are uncommon.
 *       The returned object must still be checked by the caller because NULL
 *       means allocation failed when @p browse was non-NULL.
 */
mdns_browse_sync_t *mdns_priv_browse_ensure_sync(mdns_browse_t *browse, mdns_browse_sync_t *sync)
{
    if (!browse) {
        return sync;
    }
    if (!sync) {
        sync = (mdns_browse_sync_t *)mdns_mem_malloc(sizeof(mdns_browse_sync_t));
        if (!sync) {
            HOOK_MALLOC_FAILED;
            return NULL;
        }
        sync->browse = browse;
        sync->sync_result = NULL;
    }
    return sync;
}

void mdns_priv_browse_staged_ip_free(mdns_browse_staged_ip_t *staged)
{
    while (staged) {
        mdns_browse_staged_ip_t *next = staged->next;
        mdns_mem_free(staged);
        staged = next;
    }
}

esp_err_t mdns_priv_browse_stage_ip(mdns_browse_staged_ip_t **staged, const char *hostname, esp_ip_addr_t *ip,
                                    mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl)
{
    mdns_browse_staged_ip_t *item = (mdns_browse_staged_ip_t *)mdns_mem_malloc(sizeof(mdns_browse_staged_ip_t));
    if (!item) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }
    memset(item, 0, sizeof(mdns_browse_staged_ip_t));
    strncpy(item->hostname, hostname, MDNS_NAME_BUF_LEN - 1);
    item->hostname[MDNS_NAME_BUF_LEN - 1] = '\0';
    item->ip = *ip;
    item->tcpip_if = tcpip_if;
    item->ip_protocol = ip_protocol;
    item->ttl = ttl;
    item->next = *staged;
    *staged = item;
    return ESP_OK;
}

/**
 * @brief Apply packet-staged A/AAAA records after SRV hostnames are known
 *
 * @note Each staged address is applied via mdns_priv_browse_result_add_ip(), which
 *       attaches the address only to the first browse result with a matching
 *       hostname on the same interface and IP protocol. Additional instances that
 *       share the same target host do not receive a copy automatically.
 */
void mdns_priv_browse_apply_staged_ips(mdns_browse_t *browse, mdns_browse_staged_ip_t *staged,
                                       mdns_browse_sync_t *out_sync_browse)
{
    if (!browse || !staged || !out_sync_browse || out_sync_browse->browse != browse) {
        return;
    }
    while (staged) {
        mdns_priv_browse_result_add_ip(browse, staged->hostname, &staged->ip, staged->tcpip_if,
                                       staged->ip_protocol, staged->ttl, out_sync_browse);
        staged = staged->next;
    }
}

void mdns_priv_browse_result_add_ptr(mdns_browse_t *browse, const char *instance, const char *service, const char *proto,
                                     mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl,
                                     mdns_browse_sync_t *out_sync_browse)
{
    if (!browse || !out_sync_browse || out_sync_browse->browse != browse
            || mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service)
            || mdns_utils_str_null_or_empty(proto)) {
        return;
    }
    mdns_result_t *r = browse->result;
    while (r) {
        if (r->esp_netif == mdns_priv_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol &&
                !mdns_utils_str_null_or_empty(r->instance_name) && !strcasecmp(instance, r->instance_name) &&
                !mdns_utils_str_null_or_empty(r->service_type) && !strcasecmp(service, r->service_type) &&
                !mdns_utils_str_null_or_empty(r->proto) && !strcasecmp(proto, r->proto)) {
            if (r->ttl != ttl) {
                uint32_t previous_ttl = r->ttl;
                if (r->ttl == 0) {
                    r->ttl = ttl;
                } else {
                    mdns_priv_query_update_result_ttl(r, ttl);
                }
                if (previous_ttl != r->ttl) {
                    add_browse_result(out_sync_browse, r);
                }
            }
            return;
        }
        r = r->next;
    }

    r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
    if (!r) {
        HOOK_MALLOC_FAILED;
        return;
    }
    memset(r, 0, sizeof(mdns_result_t));
    r->instance_name = mdns_mem_strdup(instance);
    r->service_type = mdns_mem_strdup(service);
    r->proto = mdns_mem_strdup(proto);
    if (!r->instance_name || !r->service_type || !r->proto) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(r->instance_name);
        mdns_mem_free(r->service_type);
        mdns_mem_free(r->proto);
        mdns_mem_free(r);
        return;
    }
    r->esp_netif = mdns_priv_get_esp_netif(tcpip_if);
    r->ip_protocol = ip_protocol;
    r->ttl = ttl;
    r->next = browse->result;
    browse->result = r;
    add_browse_result(out_sync_browse, r);
}

mdns_browse_t *mdns_priv_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_browse_t *b = s_browse;
    // For browse, we only care about the SRV, TXT, A and AAAA
    if (type != MDNS_TYPE_SRV && type != MDNS_TYPE_A && type != MDNS_TYPE_AAAA && type != MDNS_TYPE_TXT) {
        return NULL;
    }
    mdns_result_t *r = NULL;
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
            r = b->result;
            while (r) {
                if (r->esp_netif == mdns_priv_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(name->host, r->hostname)) {
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

static void sync_browse_result_link_free(mdns_browse_sync_t *browse_sync)
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

void mdns_priv_browse_sync_free(mdns_browse_sync_t *browse_sync)
{
    if (!browse_sync) {
        return;
    }
    sync_browse_result_link_free(browse_sync);
}

void mdns_priv_browse_action(mdns_action_t *action, mdns_action_subtype_t type)
{
    if (type == ACTION_RUN) {
        switch (action->type) {
        case ACTION_BROWSE_START:
            browse_start(action->data.browse_add.browse);
            break;
        case ACTION_BROWSE_SYNC:
            browse_sync(action->data.browse_sync.browse_sync);
            sync_browse_result_link_free(action->data.browse_sync.browse_sync);
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
        case ACTION_BROWSE_SYNC:
            sync_browse_result_link_free(action->data.browse_sync.browse_sync);
            break;
        default:
            abort();
        }
        return;
    }

}

/**
 * @brief  Add result to browse, only add when the result is a new one.
 */
static esp_err_t add_browse_result(mdns_browse_sync_t *sync_browse, mdns_result_t *r)
{
    mdns_browse_result_sync_t *sync_r = sync_browse->sync_result;
    while (sync_r) {
        if (sync_r->result == r) {
            break;
        }
        sync_r = sync_r->next;
    }
    if (!sync_r) {
        // Do not find, need to add the result to the list
        mdns_browse_result_sync_t *new = (mdns_browse_result_sync_t *)mdns_mem_malloc(sizeof(mdns_browse_result_sync_t));

        if (!new) {
            HOOK_MALLOC_FAILED;
            return ESP_ERR_NO_MEM;
        }
        new->result = r;
        new->next = sync_browse->sync_result;
        sync_browse->sync_result = new;
    }
    return ESP_OK;
}

/**
 * @brief  Called from parser to add A/AAAA data to browse result
 *
 * @note Only the first browse result with a matching @p hostname (same interface
 *       and IP protocol) receives the address. This predates browse staging and
 *       also limits mdns_priv_browse_apply_staged_ips() when several instances
 *       share one target host.
 */
void mdns_priv_browse_result_add_ip(mdns_browse_t *browse, const char *hostname, esp_ip_addr_t *ip,
                                    mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse)
{
    if (out_sync_browse->browse == NULL) {
        return;
    } else {
        if (out_sync_browse->browse != browse) {
            return;
        }
    }
    mdns_result_t *r = NULL;
    mdns_ip_addr_t *r_a = NULL;
    if (browse) {
        r = browse->result;
        while (r) {
            if (r->ip_protocol == ip_protocol) {
                // Find the target result in browse result.
                if (r->esp_netif == mdns_priv_get_esp_netif(tcpip_if) && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(hostname, r->hostname)) {
                    r_a = r->addr;
                    // Check if the address has already added in result.
                    while (r_a) {
#ifdef CONFIG_LWIP_IPV4
                        if (r_a->addr.type == ip->type && r_a->addr.type == ESP_IPADDR_TYPE_V4 && r_a->addr.u_addr.ip4.addr == ip->u_addr.ip4.addr) {
                            break;
                        }
#endif
#ifdef CONFIG_LWIP_IPV6
                        if (r_a->addr.type == ip->type && r_a->addr.type == ESP_IPADDR_TYPE_V6 && !memcmp(r_a->addr.u_addr.ip6.addr, ip->u_addr.ip6.addr, 16)) {
                            break;
                        }
#endif
                        r_a = r_a->next;
                    }
                    if (!r_a) {
                        // The current IP is a new one, add it to the link list.
                        mdns_ip_addr_t *a = NULL;
                        a = mdns_priv_result_addr_create_ip(ip);
                        if (!a) {
                            return;
                        }
                        a->next = r->addr;
                        r->addr = a;
                        if (r->ttl != ttl) {
                            if (r->ttl == 0) {
                                r->ttl = ttl;
                            } else {
                                mdns_priv_query_update_result_ttl(r, ttl);
                            }
                        }
                        if (add_browse_result(out_sync_browse, r) != ESP_OK) {
                            return;
                        }
                        break;
                    }
                }
            }
            r = r->next;
        }
    }
}

static bool txt_values_equal(const char *a, const char *b, uint8_t len)
{
    if (len == 0) {
        return true;
    }
    if (!a || !b) {
        return a == b;
    }
    return memcmp(a, b, len) == 0;
}

static bool is_txt_item_in_list(mdns_txt_item_t txt, uint8_t txt_value_len, mdns_txt_item_t *txt_list, uint8_t *txt_value_len_list, size_t txt_count)
{
    for (size_t i = 0; i < txt_count; i++) {
        if (mdns_utils_str_null_or_empty(txt.key) || mdns_utils_str_null_or_empty(txt_list[i].key)) {
            if (mdns_utils_str_null_or_empty(txt.key) != mdns_utils_str_null_or_empty(txt_list[i].key)) {
                continue;
            }
        } else if (strcmp(txt.key, txt_list[i].key) != 0) {
            continue;
        }
        if (txt_value_len != txt_value_len_list[i]) {
            return false;
        }
        if (txt_values_equal(txt.value, txt_list[i].value, txt_value_len)) {
            return true;
        }
        // The key value is unique, so there is no need to continue searching.
        return false;
    }
    return false;
}

/**
 * @brief  Called from parser to add TXT data to search result
 */
void mdns_priv_browse_result_add_txt(mdns_browse_t *browse, const char *instance, const char *service, const char *proto,
                                     mdns_txt_item_t *txt, uint8_t *txt_value_len, size_t txt_count, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol,
                                     uint32_t ttl, mdns_browse_sync_t *out_sync_browse)
{
    if (out_sync_browse->browse == NULL || out_sync_browse->browse != browse
            || mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service)
            || mdns_utils_str_null_or_empty(proto)) {
        goto free_txt;
    }
    mdns_result_t *r = browse->result;
    while (r) {
        if (r->esp_netif == mdns_priv_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol &&
                !mdns_utils_str_null_or_empty(r->instance_name) && !strcasecmp(instance, r->instance_name) &&
                !mdns_utils_str_null_or_empty(r->service_type) && !strcasecmp(service, r->service_type) &&
                !mdns_utils_str_null_or_empty(r->proto) && !strcasecmp(proto, r->proto)) {
            bool should_update = false;
            if (r->txt) {
                // Check if txt changed
                if (txt_count != r->txt_count) {
                    should_update = true;
                } else {
                    for (size_t txt_index = 0; txt_index < txt_count; txt_index++) {
                        if (!is_txt_item_in_list(txt[txt_index], txt_value_len[txt_index], r->txt, r->txt_value_len, r->txt_count)) {
                            should_update = true;
                            break;
                        }
                    }
                }
                // If the result has a previous txt entry, we delete it and re-add.
                for (size_t i = 0; i < r->txt_count; i++) {
                    mdns_mem_free((char *)(r->txt[i].key));
                    mdns_mem_free((char *)(r->txt[i].value));
                }
                mdns_mem_free(r->txt);
                mdns_mem_free(r->txt_value_len);
            }
            r->txt = txt;
            r->txt_value_len = txt_value_len;
            r->txt_count = txt_count;
            if (r->ttl != ttl) {
                uint32_t previous_ttl = r->ttl;
                if (r->ttl == 0) {
                    r->ttl = ttl;
                } else {
                    mdns_priv_query_update_result_ttl(r, ttl);
                }
                if (previous_ttl != r->ttl) {
                    should_update = true;
                }
            }
            if (should_update) {
                if (add_browse_result(out_sync_browse, r) != ESP_OK) {
                    return;
                }
            }
            return;
        }
        r = r->next;
    }
    r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
    if (!r) {
        HOOK_MALLOC_FAILED;
        goto free_txt;
    }
    memset(r, 0, sizeof(mdns_result_t));
    r->instance_name = mdns_mem_strdup(instance);
    r->service_type = mdns_mem_strdup(service);
    r->proto = mdns_mem_strdup(proto);
    if (!r->instance_name || !r->service_type || !r->proto) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(r->instance_name);
        mdns_mem_free(r->service_type);
        mdns_mem_free(r->proto);
        mdns_mem_free(r);
        goto free_txt;
    }
    r->txt = txt;
    r->txt_value_len = txt_value_len;
    r->txt_count = txt_count;
    r->esp_netif = mdns_priv_get_esp_netif(tcpip_if);
    r->ip_protocol = ip_protocol;
    r->ttl = ttl;
    r->next = browse->result;
    browse->result = r;
    add_browse_result(out_sync_browse, r);
    return;

free_txt:
    for (size_t i = 0; i < txt_count; i++) {
        mdns_mem_free((char *)(txt[i].key));
        mdns_mem_free((char *)(txt[i].value));
    }
    mdns_mem_free(txt);
    mdns_mem_free(txt_value_len);
    return;
}

static esp_err_t copy_address_in_previous_result(mdns_result_t *result_list, mdns_result_t *r)
{
    while (result_list) {
        if (!mdns_utils_str_null_or_empty(result_list->hostname) && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(result_list->hostname, r->hostname) &&
                result_list->ip_protocol == r->ip_protocol && result_list->addr && !r->addr) {
            // If there is a same hostname in previous result, we need to copy the address here.
            r->addr = mdns_utils_copy_address_list(result_list->addr);
            if (!r->addr) {
                return ESP_ERR_NO_MEM;
            }
            break;
        } else {
            result_list = result_list->next;
        }
    }
    return ESP_OK;
}

/**
 * @brief  Called from parser to add SRV data to search result
 */
void mdns_priv_browse_result_add_srv(mdns_browse_t *browse, const char *hostname, const char *instance, const char *service, const char *proto,
                                     uint16_t port, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse)
{
    if (out_sync_browse->browse == NULL) {
        return;
    } else {
        if (out_sync_browse->browse != browse) {
            return;
        }
    }
    if (mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service)
            || mdns_utils_str_null_or_empty(proto)) {
        return;
    }
    mdns_result_t *r = browse->result;
    while (r) {
        if (r->esp_netif == mdns_priv_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol &&
                !mdns_utils_str_null_or_empty(r->instance_name) && !strcasecmp(instance, r->instance_name) &&
                !mdns_utils_str_null_or_empty(r->service_type) && !strcasecmp(service, r->service_type) &&
                !mdns_utils_str_null_or_empty(r->proto) && !strcasecmp(proto, r->proto)) {
            if (mdns_utils_str_null_or_empty(r->hostname)
                    || mdns_utils_str_null_or_empty(hostname)
                    || strcasecmp(hostname, r->hostname)) {
                mdns_mem_free((char *)r->hostname);
                r->hostname = mdns_mem_strdup(hostname);
                r->port = port;
                if (!r->hostname) {
                    HOOK_MALLOC_FAILED;
                    return;
                }
                if (!r->addr) {
                    esp_err_t err = copy_address_in_previous_result(browse->result, r);
                    if (err == ESP_ERR_NO_MEM) {
                        return;
                    }
                }
                if (add_browse_result(out_sync_browse, r) != ESP_OK) {
                    return;
                }
            }
            if (r->ttl != ttl) {
                uint32_t previous_ttl = r->ttl;
                if (r->ttl == 0) {
                    r->ttl = ttl;
                } else {
                    mdns_priv_query_update_result_ttl(r, ttl);
                }
                if (previous_ttl != r->ttl) {
                    if (add_browse_result(out_sync_browse, r) != ESP_OK) {
                        return;
                    }
                }
            }
            return;
        }
        r = r->next;
    }
    r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
    if (!r) {
        HOOK_MALLOC_FAILED;
        return;
    }

    memset(r, 0, sizeof(mdns_result_t));
    r->hostname = mdns_mem_strdup(hostname);
    r->instance_name = mdns_mem_strdup(instance);
    r->service_type = mdns_mem_strdup(service);
    r->proto = mdns_mem_strdup(proto);
    if (!r->hostname || !r->instance_name || !r->service_type || !r->proto) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(r->hostname);
        mdns_mem_free(r->instance_name);
        mdns_mem_free(r->service_type);
        mdns_mem_free(r->proto);
        mdns_mem_free(r);
        return;
    }
    r->port = port;
    r->esp_netif = mdns_priv_get_esp_netif(tcpip_if);
    r->ip_protocol = ip_protocol;
    r->ttl = ttl;
    r->next = browse->result;
    browse->result = r;
    add_browse_result(out_sync_browse, r);
}

/**
 * @brief  Browse sync result
 */
esp_err_t mdns_priv_browse_sync(mdns_browse_sync_t *browse_sync)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = ACTION_BROWSE_SYNC;
    action->data.browse_sync.browse_sync = browse_sync;
    if (!mdns_priv_queue_action(action)) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @defgroup MDNS_PUBCLIC_API
 */
mdns_browse_t *mdns_browse_new(const char *service, const char *proto, mdns_browse_notify_t notifier)
{
    mdns_browse_t *browse = NULL;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }

    browse = browse_init(service, proto, notifier);
    if (!browse) {
        return NULL;
    }

    mdns_priv_service_lock();
    // Check if the browse already exists before sending action
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_RUNNING
                && strlen(it->service) == strlen(browse->service) && memcmp(it->service, browse->service, strlen(it->service)) == 0
                && strlen(it->proto) == strlen(browse->proto) && memcmp(it->proto, browse->proto, strlen(it->proto)) == 0) {
            ESP_LOGW(TAG, "Browse already exists: %s.%s", browse->service, browse->proto);
            goto error;
        }
    }

    if (send_browse_action(ACTION_BROWSE_START, browse) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send browse start action");
        goto error;
    }

    browse->state = BROWSE_RUNNING;
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
    mdns_browse_t *browse = NULL;

    if (!mdns_priv_is_server_init() || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_FAIL;
    }

    mdns_priv_service_lock();
    for (mdns_browse_t *it = s_browse; it; it = it->next) {
        if (it->state == BROWSE_RUNNING
                && strlen(it->service) == strlen(service) && memcmp(it->service, service, strlen(it->service)) == 0
                && strlen(it->proto) == strlen(proto) && memcmp(it->proto, proto, strlen(it->proto)) == 0) {
            browse = it;
            break;
        }
    }

    if (!browse) {
        mdns_priv_service_unlock();
        return ESP_FAIL;
    }

    browse->state = BROWSE_OFF;

    if (send_browse_action(ACTION_BROWSE_END, browse) != ESP_OK) {
        browse->state = BROWSE_RUNNING;
        mdns_priv_service_unlock();
        return ESP_ERR_NO_MEM;
    }

    mdns_priv_service_unlock();
    return ESP_OK;
}
