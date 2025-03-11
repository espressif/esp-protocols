/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mdns_private.h"

#define MDNS_UTILS_DEFAULT_DOMAIN "local"
#define _MDNS_SIZEOF_IP6_ADDR (MDNS_ANSWER_AAAA_SIZE)

/**
 * @brief  read uint16_t from a packet
 * @param  packet       the packet
 * @param  index        index in the packet where the value starts
 *
 * @return the value
 */
static inline uint16_t mdns_utils_read_u16(const uint8_t *packet, uint16_t index)
{
    return (uint16_t)(packet[index]) << 8 | packet[index + 1];
}

/**
 * @brief  read uint32_t from a packet
 * @param  packet       the packet
 * @param  index        index in the packet where the value starts
 *
 * @return the value
 */
static inline uint32_t mdns_utils_read_u32(const uint8_t *packet, uint16_t index)
{
    return (uint32_t)(packet[index]) << 24 | (uint32_t)(packet[index + 1]) << 16 | (uint32_t)(packet[index + 2]) << 8 | packet[index + 3];
}

static inline bool mdns_utils_str_null_or_empty(const char *str)
{
    return (str == NULL || *str == 0);
}

static inline void _mdns_result_update_ttl(mdns_result_t *r, uint32_t ttl)
{
    r->ttl = r->ttl < ttl ? r->ttl : ttl;
}


const uint8_t *_mdns_parse_fqdn(const uint8_t *packet, const uint8_t *start, mdns_name_t *name, size_t packet_len);

const uint8_t *_mdns_read_fqdn(const uint8_t *packet, const uint8_t *start, mdns_name_t *name, char *buf, size_t packet_len);

bool is_mdns_server(void);
//mdns_pcb_t *mdns_utils_get_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
const char *mdns_utils_get_global_hostname(void);
mdns_srv_item_t *mdns_utils_get_services(void);
mdns_host_item_t *mdns_utils_get_hosts(void);
mdns_host_item_t *mdns_utils_get_selfhost(void);
void mdns_utils_set_global_hostname(const char *hostname);
const char *mdns_utils_get_instance(void);
void mdns_utils_set_instance(const char *instance);
//mdns_search_once_t *mdns_utils_get_search(void);
//mdns_browse_t *mdns_utils_get_browse(void);
//mdns_tx_packet_t *mdns_utils_get_tx_packet(void);
//bool mdns_utils_is_probing(mdns_rx_packet_t *packet);
//bool mdns_utils_is_pcb_after_init(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
//bool mdns_utils_is_pcb_duplicate(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
//bool mdns_utils_after_probing(mdns_rx_packet_t *packet);
//void mdns_utils_probe_failed(mdns_rx_packet_t *packet);

void _mdns_schedule_tx_packet(mdns_tx_packet_t *packet, uint32_t ms_after);
void _mdns_probe_all_pcbs(mdns_srv_item_t **services, size_t len, bool probe_ip, bool clear_old_probe);
void _mdns_restart_all_pcbs_no_instance(void);
void _mdns_remap_self_service_hostname(const char *old_hostname, const char *new_hostname);
void _mdns_restart_all_pcbs(void);
void _mdns_pcb_send_bye(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool include_ip);
void _mdns_init_pcb_probe(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool probe_ip);
bool mdns_utils_ipv6_address_is_zero(esp_ip6_addr_t ip6);


void _mdns_create_answer_from_parsed_packet(mdns_parsed_packet_t *parsed_packet);

bool _hostname_is_ours(const char *hostname);
bool _mdns_service_match(const mdns_service_t *srv, const char *service, const char *proto,
                         const char *hostname);
mdns_srv_item_t *_mdns_get_service_item(const char *service, const char *proto, const char *hostname);

mdns_srv_item_t *_mdns_get_service_item_instance(const char *instance, const char *service, const char *proto,
                                                 const char *hostname);

bool _mdns_service_match_instance(const mdns_service_t *srv, const char *instance, const char *service,
                                  const char *proto, const char *hostname);

bool _mdns_instance_name_match(const char *lhs, const char *rhs);

const char *_mdns_get_default_instance_name(void);
const char *_mdns_get_service_instance_name(const mdns_service_t *service);
esp_err_t _mdns_add_browse_result(mdns_browse_sync_t *sync_browse, mdns_result_t *r);
mdns_ip_addr_t *copy_address_list(const mdns_ip_addr_t *address_list);
void free_address_list(mdns_ip_addr_t *address_list);
int append_one_txt_record_entry(uint8_t *packet, uint16_t *index, mdns_txt_linked_item_t *txt);
uint8_t _mdns_append_u16(uint8_t *packet, uint16_t *index, uint16_t value);
//---------------------------------------------


///
//mdns_if_t _mdns_get_other_if(mdns_if_t tcpip_if);
//void _mdns_dup_interface(mdns_if_t tcpip_if);

void mdns_service_lock(void);
void mdns_service_unlock(void);

bool mdns_action_queue(mdns_action_t *action);
