/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "mdns_private.h"

#ifdef __cplusplus
extern "C" {
#endif

void _mdns_announce_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool include_ip);
bool mdns_responder_is_off(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
void mdns_responder_process_tx_packet(mdns_tx_packet_t *p);
void mdns_responder_check_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_service_t *service, bool removed_answers, bool *should_remove_questions);
void mdns_responder_deinit(void);
bool mdsn_responder_iface_init(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
bool mdns_responder_is_duplicate(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
bool mdns_responder_is_probing(mdns_rx_packet_t *packet);
void mdns_responder_probe_failed(mdns_rx_packet_t *packet);
bool mdns_responder_after_probing(mdns_rx_packet_t *packet);

void _mdns_send_bye(mdns_srv_item_t **services, size_t len, bool include_ip);
#ifdef __cplusplus
}
#endif
