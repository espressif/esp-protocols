/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * MDNS Dependecy injection -- preincluded to inject interface test functions into static variables
 *
 */

#include "mdns.h"
#include "mdns_private.h"

esp_err_t (*mdns_test_static_send_search_action)(mdns_action_type_t type, mdns_search_once_t *search) = NULL;
void (*mdns_test_static_search_free)(mdns_search_once_t *search) = NULL;

mdns_search_once_t *(*mdns_test_static_search_init)(const char *name, const char *service, const char *proto, uint16_t type, bool unicast,
                                                    uint32_t timeout, uint8_t max_results,
                                                    mdns_query_notify_t notifier) = NULL;

static mdns_search_once_t *_mdns_search_init(const char *name, const char *service, const char *proto, uint16_t type, bool unicast,
                                             uint32_t timeout, uint8_t max_results, mdns_query_notify_t notifier);
static esp_err_t _mdns_send_search_action(mdns_action_type_t type, mdns_search_once_t *search);
static void _mdns_search_free(mdns_search_once_t *search);

void mdns_querier_test_init_di(void)
{
    mdns_test_static_search_init = _mdns_search_init;
    mdns_test_static_send_search_action = _mdns_send_search_action;
    mdns_test_static_search_free = _mdns_search_free;
}


mdns_search_once_t *mdns_test_search_init(const char *name, const char *service, const char *proto, uint16_t type, uint32_t timeout, uint8_t max_results)
{
    return mdns_test_static_search_init(name, service, proto, type, timeout, type != MDNS_TYPE_PTR, max_results, NULL);
}

void mdns_test_search_free(mdns_search_once_t *search)
{
    return mdns_test_static_search_free(search);
}

esp_err_t mdns_test_send_search_action(mdns_action_type_t type, mdns_search_once_t *search)
{
    return mdns_test_static_send_search_action(type, search);
}
