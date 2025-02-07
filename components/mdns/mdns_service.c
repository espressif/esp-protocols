/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_types.h"

// Service management functions
static esp_err_t mdns_start_service_task(void);
static void mdns_service_task(void *pvParameters);
static void mdns_timer_cb(void *arg);

// Action handling
static esp_err_t mdns_handle_system_event(mdns_action_t *action);
static esp_err_t mdns_handle_action(mdns_action_t *action);
