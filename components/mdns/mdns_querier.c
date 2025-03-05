/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_types.h"

// Query functions
static esp_err_t mdns_send_query(mdns_out_question_t *questions);
static void mdns_query_handle_response(mdns_parsed_packet_t *packet);
static void mdns_search_finish(mdns_search_once_t *search);
