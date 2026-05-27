/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "mdns_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  Free browse item queue
 *
 *  @note Called from mdns_free()
 */
void mdns_priv_browse_free(void);

/**
 *  @brief Looks for the name/type in active browse items
 *
 *  @note Called from the packet parser (mdns_receive.c)
 *
 *  @return browse results
 */
mdns_browse_t *mdns_priv_browse_find(mdns_name_t *name, uint16_t type, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

/**
 * @brief Looks for an active browse matching a PTR owner name (service._proto.local)
 *
 * @note Called from the packet parser (mdns_receive.c)
 */
mdns_browse_t *mdns_priv_browse_find_ptr(mdns_name_t *name);

/**
 * @brief Packet-scoped staging for browse A/AAAA records received before SRV
 */
typedef struct mdns_browse_staged_ip {
    struct mdns_browse_staged_ip *next;
    char hostname[MDNS_NAME_BUF_LEN];
    esp_ip_addr_t ip;
    mdns_if_t tcpip_if;
    mdns_ip_protocol_t ip_protocol;
    uint32_t ttl;
} mdns_browse_staged_ip_t;

/**
 * @brief Allocate or return existing browse sync object for a packet
 */
mdns_browse_sync_t *mdns_priv_browse_ensure_sync(mdns_browse_t *browse, mdns_browse_sync_t *sync);

/**
 * @brief Free browse sync object and its pending result list
 */
void mdns_priv_browse_sync_free(mdns_browse_sync_t *browse_sync);

/**
 * @brief Stage an A/AAAA record to apply after all packet records are parsed
 */
esp_err_t mdns_priv_browse_stage_ip(mdns_browse_staged_ip_t **staged, const char *hostname, esp_ip_addr_t *ip,
                                    mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl);

/**
 * @brief Apply staged A/AAAA records once SRV/hostnames are known
 *
 * @note Delegates to mdns_priv_browse_result_add_ip(), which updates only the
 *       first matching instance per hostname; see mdns_browse_new() in mdns.h.
 */
void mdns_priv_browse_apply_staged_ips(mdns_browse_t *browse, mdns_browse_staged_ip_t *staged,
                                       mdns_browse_sync_t *out_sync_browse);

/**
 * @brief Free staged A/AAAA list
 */
void mdns_priv_browse_staged_ip_free(mdns_browse_staged_ip_t *staged);

/**
 * @brief Add a PTR record to the browse result (including TTL=0 removal)
 */
void mdns_priv_browse_result_add_ptr(mdns_browse_t *browse, const char *instance, const char *service, const char *proto,
                                     mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl,
                                     mdns_browse_sync_t *out_sync_browse);

/**
 * @brief Send out all browse queries
 *
 * @note Called from the network events (mdns_netif.c)
 * @note Calls (indirectly) search-send from mdns_querier.c, which sends out the query
 */
void mdns_priv_browse_send_all(mdns_if_t mdns_if);

/**
 * @brief Send out browse queries by IP protocol
 *
 * @note Called from the network events (mdns_netif.c)
 * @note Calls (indirectly) search-send from mdns_querier.c, which sends out the query
 */
void mdns_priv_browse_send_by_ip_protocol(mdns_if_t mdns_if, mdns_ip_protocol_t ip_protocol);

/**
 * @brief Sync browse results
 *
 * @note Called from the packet parser
 * @note Calls mdns_priv_queue_action() from mdns_engine
 */
esp_err_t mdns_priv_browse_sync(mdns_browse_sync_t *browse_sync);

/**
 * @brief Perform action from mdns service queue
 *
 * @note Called from the _mdns_service_task() in mdns.c
 */
void mdns_priv_browse_action(mdns_action_t *action, mdns_action_subtype_t type);

/**
 * @brief  Add a TXT record to the browse result
 *
 * @note Called from the packet parser (mdns_receive.c)
 */
void mdns_priv_browse_result_add_txt(mdns_browse_t *browse, const char *instance, const char *service, const char *proto,
                                     mdns_txt_item_t *txt, uint8_t *txt_value_len, size_t txt_count, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol,
                                     uint32_t ttl, mdns_browse_sync_t *out_sync_browse);
/**
 * @brief  Add an IP record to the browse result
 *
 * @note Called from the packet parser (mdns_receive.c)
 * @note Attaches @p ip only to the first browse result with matching @p hostname
 *       on the given interface and IP protocol; see mdns_browse_new() in mdns.h.
 */
void mdns_priv_browse_result_add_ip(mdns_browse_t *browse, const char *hostname, esp_ip_addr_t *ip,
                                    mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse);
/**
 * @brief  Add a SRV record to the browse result
 *
 * @note Called from the packet parser (mdns_receive.c)
 */
void mdns_priv_browse_result_add_srv(mdns_browse_t *browse, const char *hostname, const char *instance, const char *service, const char *proto,
                                     uint16_t port, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse);
#ifdef __cplusplus
}
#endif
