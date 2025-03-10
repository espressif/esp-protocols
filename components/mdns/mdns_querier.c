/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "mdns_private.h"
#include "mdns_querier.h"
#include "mdns_mem_caps.h"
#include "mdns_utils.h"
#include "mdns_send.h"
#include "esp_log.h"
#include "mdns_responder.h"

static const char *TAG = "mdns_querier";

static mdns_search_once_t *s_search_once;

void _mdns_query_results_free(mdns_result_t *results)
{
    mdns_result_t *r;
    mdns_ip_addr_t *a;

    while (results) {
        r = results;

        mdns_mem_free((char *)(r->hostname));
        mdns_mem_free((char *)(r->instance_name));
        mdns_mem_free((char *)(r->service_type));
        mdns_mem_free((char *)(r->proto));

        for (size_t i = 0; i < r->txt_count; i++) {
            mdns_mem_free((char *)(r->txt[i].key));
            mdns_mem_free((char *)(r->txt[i].value));
        }
        mdns_mem_free(r->txt);
        mdns_mem_free(r->txt_value_len);

        while (r->addr) {
            a = r->addr;
            r->addr = r->addr->next;
            mdns_mem_free(a);
        }

        results = results->next;
        mdns_mem_free(r);
    }
}

/**
 * @brief  Mark search as finished and remove it from search chain
 */
void _mdns_search_finish(mdns_search_once_t *search)
{
    search->state = SEARCH_OFF;
    queueDetach(mdns_search_once_t, s_search_once, search);
    if (search->notifier) {
        search->notifier(search);
    }
    xSemaphoreGive(search->done_semaphore);
}

/**
 * @brief  Add new search to the search chain
 */
void _mdns_search_add(mdns_search_once_t *search)
{
    search->next = s_search_once;
    s_search_once = search;
}

/**
 * @brief  Called from timer task to run active searches
 */
void _mdns_search_run(void)
{
    mdns_service_lock();
    mdns_search_once_t *s = s_search_once;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (!s) {
        mdns_service_unlock();
        return;
    }
    while (s) {
        if (s->state != SEARCH_OFF) {
            if (now > (s->started_at + s->timeout)) {
                s->state = SEARCH_OFF;
                if (_mdns_send_search_action(ACTION_SEARCH_END, s) != ESP_OK) {
                    s->state = SEARCH_RUNNING;
                }
            } else if (s->state == SEARCH_INIT || (now - s->sent_at) > 1000) {
                s->state = SEARCH_RUNNING;
                s->sent_at = now;
                if (_mdns_send_search_action(ACTION_SEARCH_SEND, s) != ESP_OK) {
                    s->sent_at -= 1000;
                }
            }
        }
        s = s->next;
    }
    mdns_service_unlock();
}

void mdns_search_free(void)
{
    while (s_search_once) {
        mdns_search_once_t *h = s_search_once;
        s_search_once = h->next;
        mdns_mem_free(h->instance);
        mdns_mem_free(h->service);
        mdns_mem_free(h->proto);
        vSemaphoreDelete(h->done_semaphore);
        if (h->result) {
            _mdns_query_results_free(h->result);
        }
        mdns_mem_free(h);
    }
}

/**
 * @brief  Send search packet to all available interfaces
 */
void _mdns_search_send(mdns_search_once_t *search)
{
    mdns_search_once_t *queue = s_search_once;
    bool found = false;
    // looking for this search in active searches
    while (queue) {
        if (queue == search) {
            found = true;
            break;
        }
        queue = queue->next;
    }

    if (!found) {
        // no longer active -> skip sending this search
        return;
    }

    uint8_t i, j;
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            _mdns_search_send_pcb(search, (mdns_if_t)i, (mdns_ip_protocol_t)j);
        }
    }
}

/**
 * @brief  Called from parser to finish any searches that have reached maximum results
 */
void _mdns_search_finish_done(void)
{
    mdns_search_once_t *search = s_search_once;
    mdns_search_once_t *s = NULL;
    while (search) {
        s = search;
        search = search->next;
        if (s->max_results && s->num_results >= s->max_results) {
            _mdns_search_finish(s);
        }
    }
}

/**
 * @brief  Called from packet parser to find matching running search
 */
mdns_search_once_t *_mdns_search_find_from(mdns_search_once_t *s, mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_result_t *r = NULL;
    while (s) {
        if (s->state == SEARCH_OFF) {
            s = s->next;
            continue;
        }

        if (type == MDNS_TYPE_A || type == MDNS_TYPE_AAAA) {
            if ((s->type == MDNS_TYPE_ANY && s->service != NULL)
                    || (s->type != MDNS_TYPE_ANY && s->type != type && s->type != MDNS_TYPE_PTR && s->type != MDNS_TYPE_SRV)) {
                s = s->next;
                continue;
            }
            if (s->type != MDNS_TYPE_PTR && s->type != MDNS_TYPE_SRV) {
                if (!strcasecmp(name->host, s->instance)) {
                    return s;
                }
                s = s->next;
                continue;
            }
            r = s->result;
            while (r) {
                if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(name->host, r->hostname)) {
                    return s;
                }
                r = r->next;
            }
            s = s->next;
            continue;
        }

        if (type == MDNS_TYPE_SRV || type == MDNS_TYPE_TXT) {
            if ((s->type == MDNS_TYPE_ANY && s->service == NULL)
                    || (s->type != MDNS_TYPE_ANY && s->type != type && s->type != MDNS_TYPE_PTR)) {
                s = s->next;
                continue;
            }
            if (strcasecmp(name->service, s->service)
                    || strcasecmp(name->proto, s->proto)) {
                s = s->next;
                continue;
            }
            if (s->type != MDNS_TYPE_PTR) {
                if (s->instance && strcasecmp(name->host, s->instance) == 0) {
                    return s;
                }
                s = s->next;
                continue;
            }
            return s;
        }

        if (type == MDNS_TYPE_PTR && type == s->type && !strcasecmp(name->service, s->service) && !strcasecmp(name->proto, s->proto)) {
            return s;
        }

        s = s->next;
    }

    return NULL;
}

mdns_search_once_t *_mdns_search_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    return _mdns_search_find_from(s_search_once, name, type, tcpip_if, ip_protocol);
}

/**
 * @brief  Create search packet for particular interface
 */
static mdns_tx_packet_t *_mdns_create_search_packet(mdns_search_once_t *search, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_result_t *r = NULL;
    mdns_tx_packet_t *packet = _mdns_alloc_packet_default(tcpip_if, ip_protocol);
    if (!packet) {
        return NULL;
    }

    mdns_out_question_t *q = (mdns_out_question_t *)mdns_mem_malloc(sizeof(mdns_out_question_t));
    if (!q) {
        HOOK_MALLOC_FAILED;
        _mdns_free_tx_packet(packet);
        return NULL;
    }
    q->next = NULL;
    q->unicast = search->unicast;
    q->type = search->type;
    q->host = search->instance;
    q->service = search->service;
    q->proto = search->proto;
    q->domain = MDNS_UTILS_DEFAULT_DOMAIN;
    q->own_dynamic_memory = false;
    queueToEnd(mdns_out_question_t, packet->questions, q);

    if (search->type == MDNS_TYPE_PTR) {
        r = search->result;
        while (r) {
            //full record on the same interface is available
            if (r->esp_netif != _mdns_get_esp_netif(tcpip_if) || r->ip_protocol != ip_protocol || r->instance_name == NULL || r->hostname == NULL || r->addr == NULL) {
                r = r->next;
                continue;
            }
            mdns_out_answer_t *a = (mdns_out_answer_t *)mdns_mem_malloc(sizeof(mdns_out_answer_t));
            if (!a) {
                HOOK_MALLOC_FAILED;
                _mdns_free_tx_packet(packet);
                return NULL;
            }
            a->type = MDNS_TYPE_PTR;
            a->service = NULL;
            a->custom_instance = r->instance_name;
            a->custom_service = search->service;
            a->custom_proto = search->proto;
            a->bye = false;
            a->flush = false;
            a->next = NULL;
            queueToEnd(mdns_out_answer_t, packet->answers, a);
            r = r->next;
        }
    }

    return packet;
}


/**
 * @brief  Send search packet to particular interface
 */
void _mdns_search_send_pcb(mdns_search_once_t *search, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_tx_packet_t *packet = NULL;
    if (mdsn_responder_iface_init(tcpip_if, ip_protocol)) {
        packet = _mdns_create_search_packet(search, tcpip_if, ip_protocol);
        if (!packet) {
            return;
        }
        _mdns_dispatch_tx_packet(packet);
        _mdns_free_tx_packet(packet);
    }
}

/*
 * MDNS Search
 * */

/**
 * @brief  Free search structure (except the results)
 */
void _mdns_search_free(mdns_search_once_t *search)
{
    mdns_mem_free(search->instance);
    mdns_mem_free(search->service);
    mdns_mem_free(search->proto);
    vSemaphoreDelete(search->done_semaphore);
    mdns_mem_free(search);
}

/**
 * @brief  Allocate new search structure
 */
static mdns_search_once_t *_mdns_search_init(const char *name, const char *service, const char *proto, uint16_t type, bool unicast,
                                             uint32_t timeout, uint8_t max_results, mdns_query_notify_t notifier)
{
    mdns_search_once_t *search = (mdns_search_once_t *)mdns_mem_malloc(sizeof(mdns_search_once_t));
    if (!search) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }
    memset(search, 0, sizeof(mdns_search_once_t));

    search->done_semaphore = xSemaphoreCreateBinary();
    if (!search->done_semaphore) {
        mdns_mem_free(search);
        return NULL;
    }

    if (!mdns_utils_str_null_or_empty(name)) {
        search->instance = mdns_mem_strndup(name, MDNS_NAME_BUF_LEN - 1);
        if (!search->instance) {
            _mdns_search_free(search);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(service)) {
        search->service = mdns_mem_strndup(service, MDNS_NAME_BUF_LEN - 1);
        if (!search->service) {
            _mdns_search_free(search);
            return NULL;
        }
    }

    if (!mdns_utils_str_null_or_empty(proto)) {
        search->proto = mdns_mem_strndup(proto, MDNS_NAME_BUF_LEN - 1);
        if (!search->proto) {
            _mdns_search_free(search);
            return NULL;
        }
    }

    search->type = type;
    search->unicast = unicast;
    search->timeout = timeout;
    search->num_results = 0;
    search->max_results = max_results;
    search->result = NULL;
    search->state = SEARCH_INIT;
    search->sent_at = 0;
    search->started_at = xTaskGetTickCount() * portTICK_PERIOD_MS;
    search->notifier = notifier;
    search->next = NULL;

    return search;
}


/*
 * MDNS QUERY
 * */
void mdns_query_results_free(mdns_result_t *results)
{
    mdns_service_lock();
    _mdns_query_results_free(results);
    mdns_service_unlock();
}

esp_err_t mdns_query_async_delete(mdns_search_once_t *search)
{
    if (!search) {
        return ESP_ERR_INVALID_ARG;
    }
    if (search->state != SEARCH_OFF) {
        return ESP_ERR_INVALID_STATE;
    }

    mdns_service_lock();
    _mdns_search_free(search);
    mdns_service_unlock();

    return ESP_OK;
}

bool mdns_query_async_get_results(mdns_search_once_t *search, uint32_t timeout, mdns_result_t **results, uint8_t *num_results)
{
    if (xSemaphoreTake(search->done_semaphore, pdMS_TO_TICKS(timeout)) == pdTRUE) {
        if (results) {
            *results = search->result;
        }
        if (num_results) {
            *num_results = search->num_results;
        }
        return true;
    }
    return false;
}

mdns_search_once_t *mdns_query_async_new(const char *name, const char *service, const char *proto, uint16_t type,
                                         uint32_t timeout, size_t max_results, mdns_query_notify_t notifier)
{
    mdns_search_once_t *search = NULL;

    if (!is_mdns_server() || !timeout || mdns_utils_str_null_or_empty(service) != mdns_utils_str_null_or_empty(proto)) {
        return NULL;
    }

    search = _mdns_search_init(name, service, proto, type, type != MDNS_TYPE_PTR, timeout, max_results, notifier);
    if (!search) {
        return NULL;
    }

    if (_mdns_send_search_action(ACTION_SEARCH_ADD, search)) {
        _mdns_search_free(search);
        return NULL;
    }

    return search;
}

esp_err_t mdns_query_generic(const char *name, const char *service, const char *proto, uint16_t type, mdns_query_transmission_type_t transmission_type, uint32_t timeout, size_t max_results, mdns_result_t **results)
{
    mdns_search_once_t *search = NULL;

    *results = NULL;

    if (!is_mdns_server()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!timeout || mdns_utils_str_null_or_empty(service) != mdns_utils_str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    search = _mdns_search_init(name, service, proto, type, transmission_type == MDNS_QUERY_UNICAST, timeout, max_results, NULL);
    if (!search) {
        return ESP_ERR_NO_MEM;
    }

    if (_mdns_send_search_action(ACTION_SEARCH_ADD, search)) {
        _mdns_search_free(search);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(search->done_semaphore, portMAX_DELAY);

    *results = search->result;
    _mdns_search_free(search);

    return ESP_OK;
}

esp_err_t mdns_query(const char *name, const char *service_type, const char *proto, uint16_t type, uint32_t timeout, size_t max_results, mdns_result_t **results)
{
    return mdns_query_generic(name, service_type, proto, type, type != MDNS_TYPE_PTR, timeout, max_results, results);
}

esp_err_t mdns_query_ptr(const char *service, const char *proto, uint32_t timeout, size_t max_results, mdns_result_t **results)
{
    if (mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    return mdns_query(NULL, service, proto, MDNS_TYPE_PTR, timeout, max_results, results);
}

esp_err_t mdns_query_srv(const char *instance, const char *service, const char *proto, uint32_t timeout, mdns_result_t **result)
{
    if (mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    return mdns_query(instance, service, proto, MDNS_TYPE_SRV, timeout, 1, result);
}

esp_err_t mdns_query_txt(const char *instance, const char *service, const char *proto, uint32_t timeout, mdns_result_t **result)
{
    if (mdns_utils_str_null_or_empty(instance) || mdns_utils_str_null_or_empty(service) || mdns_utils_str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    return mdns_query(instance, service, proto, MDNS_TYPE_TXT, timeout, 1, result);
}
