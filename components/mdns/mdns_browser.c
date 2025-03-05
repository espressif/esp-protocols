/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_types.h"

// Browse functions
static esp_err_t mdns_browse_service(const char *service, const char *proto);
static void mdns_browse_result_add(mdns_browse_t *browse, mdns_result_t *result);
static void mdns_browse_finish(mdns_browse_t *browse);
