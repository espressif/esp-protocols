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

void _mdns_search_send_pcb(mdns_search_once_t *search, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

void _mdns_free_tx_packet(mdns_tx_packet_t *packet);

mdns_tx_packet_t *_mdns_create_probe_packet(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t *services[], size_t len, bool first, bool include_ip);

bool _mdns_alloc_answer(mdns_out_answer_t **destination, uint16_t type, mdns_service_t *service,
                        mdns_host_item_t *host, bool flush, bool bye);

mdns_tx_packet_t *_mdns_create_announce_packet(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t *services[], size_t len, bool include_ip);
mdns_tx_packet_t *_mdns_create_announce_from_probe(mdns_tx_packet_t *probe);
void _mdns_dispatch_tx_packet(mdns_tx_packet_t *p);
void _mdns_send_bye_subtype(mdns_srv_item_t *service, const char *instance_name, mdns_subtype_t *remove_subtypes);
bool _mdns_append_host_list_in_services(mdns_out_answer_t **destination, mdns_srv_item_t *services[], size_t services_len, bool flush, bool bye);
void _mdns_clear_pcb_tx_queue_head(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
void _mdns_clear_tx_queue_head(void);
void _mdns_remove_scheduled_service_packets(mdns_service_t *service);
void mdns_send_handle_tx_packet(mdns_tx_packet_t *packet);
mdns_tx_packet_t *_mdns_get_next_pcb_packet(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
mdns_tx_packet_t *_mdns_get_next_tx_packet(void);
void _mdns_dealloc_answer(mdns_out_answer_t **destination, uint16_t type, mdns_srv_item_t *service);
void _mdns_remove_scheduled_answer(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint16_t type, mdns_srv_item_t *service);
mdns_tx_packet_t *_mdns_alloc_packet_default(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

#ifdef __cplusplus
}
#endif
