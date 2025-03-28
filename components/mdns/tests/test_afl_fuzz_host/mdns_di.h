/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * MDNS Dependecy injection -- preincluded to inject interface test functions into static variables
 *
 */

#include "mdns.h"
#include "mdns_private.h"

void (*mdns_test_static_execute_action)(mdns_action_t *) = NULL;
mdns_srv_item_t *(*mdns_test_static_mdns_get_service_item)(const char *service, const char *proto, const char *hostname) = NULL;
// esp_err_t (*mdns_test_static_send_search_action)(mdns_action_type_t type, mdns_search_once_t *search) = NULL;

static void _mdns_execute_action(mdns_action_t *action);

void mdns_test_init_di(void)
{
    mdns_test_static_execute_action = _mdns_execute_action;
}

void mdns_test_execute_action(void *action)
{
    mdns_test_static_execute_action((mdns_action_t *)action);
}
