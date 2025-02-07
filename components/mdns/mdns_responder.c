/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_types.h"

// Response generation
static esp_err_t mdns_send_response(mdns_out_answer_t *answers, mdns_tx_packet_t *packet);
static void mdns_announce_service(mdns_service_t *service, bool bye);
static void mdns_announce_host(bool bye);
