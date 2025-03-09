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
const char *mdns_utils_get_global_hostname(void);
mdns_srv_item_t *mdns_utils_get_services(void);
mdns_host_item_t *mdns_utils_get_hosts(void);
mdns_host_item_t *mdns_utils_get_selfhost(void);
void mdns_utils_set_global_hostname(const char *hostname);
const char *mdns_utils_get_instance(void);
void mdns_utils_set_instance(const char *instance);
mdns_search_once_t *mdns_utils_get_search(void);
mdns_browse_t *mdns_utils_get_browse(void);
mdns_tx_packet_t *mdns_utils_get_tx_packet(void);
bool mdns_utils_is_probing(mdns_rx_packet_t *packet);
bool mdns_utils_is_pcb_after_init(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
bool mdns_utils_is_pcb_duplicate(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
bool mdns_utils_after_probing(mdns_rx_packet_t *packet);
void mdns_utils_probe_failed(mdns_rx_packet_t *packet);

void _mdns_schedule_tx_packet(mdns_tx_packet_t *packet, uint32_t ms_after);
void _mdns_probe_all_pcbs(mdns_srv_item_t **services, size_t len, bool probe_ip, bool clear_old_probe);
void _mdns_restart_all_pcbs_no_instance(void);
void _mdns_remap_self_service_hostname(const char *old_hostname, const char *new_hostname);
void _mdns_restart_all_pcbs(void);
void _mdns_pcb_send_bye(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool include_ip);
void _mdns_init_pcb_probe(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool probe_ip);
bool _ipv6_address_is_zero(esp_ip6_addr_t ip6);
void _mdns_search_finish(mdns_search_once_t *search);


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

//---------------------------------------------
/**
 * @brief  appends byte in a packet, incrementing the index
 *
 * @param  packet       MDNS packet
 * @param  index        offset in the packet
 * @param  value        the value to set
 *
 * @return length of added data: 0 on error or 1 on success
 */
static inline uint8_t _mdns_append_u8(uint8_t *packet, uint16_t *index, uint8_t value)
{
    if (*index >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    packet[*index] = value;
    *index += 1;
    return 1;
}

/**
 * @brief  appends uint16_t in a packet, incrementing the index
 *
 * @param  packet       MDNS packet
 * @param  index        offset in the packet
 * @param  value        the value to set
 *
 * @return length of added data: 0 on error or 2 on success
 */
static inline uint8_t _mdns_append_u16(uint8_t *packet, uint16_t *index, uint16_t value)
{
    if ((*index + 1) >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    _mdns_append_u8(packet, index, (value >> 8) & 0xFF);
    _mdns_append_u8(packet, index, value & 0xFF);
    return 2;
}

/**
 * @brief  appends uint32_t in a packet, incrementing the index
 *
 * @param  packet       MDNS packet
 * @param  index        offset in the packet
 * @param  value        the value to set
 *
 * @return length of added data: 0 on error or 4 on success
 */
static inline uint8_t _mdns_append_u32(uint8_t *packet, uint16_t *index, uint32_t value)
{
    if ((*index + 3) >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    _mdns_append_u8(packet, index, (value >> 24) & 0xFF);
    _mdns_append_u8(packet, index, (value >> 16) & 0xFF);
    _mdns_append_u8(packet, index, (value >> 8) & 0xFF);
    _mdns_append_u8(packet, index, value & 0xFF);
    return 4;
}

/**
 * @brief  appends answer type, class, ttl and data length to a packet, incrementing the index
 *
 * @param  packet       MDNS packet
 * @param  index        offset in the packet
 * @param  type         answer type
 * @param  ttl          answer ttl
 *
 * @return length of added data: 0 on error or 10 on success
 */
static inline uint8_t _mdns_append_type(uint8_t *packet, uint16_t *index, uint8_t type, bool flush, uint32_t ttl)
{
    if ((*index + 10) >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    uint16_t mdns_class = MDNS_CLASS_IN;
    if (flush) {
        mdns_class = MDNS_CLASS_IN_FLUSH_CACHE;
    }
    if (type == MDNS_ANSWER_PTR) {
        _mdns_append_u16(packet, index, MDNS_TYPE_PTR);
        _mdns_append_u16(packet, index, mdns_class);
    } else if (type == MDNS_ANSWER_TXT) {
        _mdns_append_u16(packet, index, MDNS_TYPE_TXT);
        _mdns_append_u16(packet, index, mdns_class);
    } else if (type == MDNS_ANSWER_SRV) {
        _mdns_append_u16(packet, index, MDNS_TYPE_SRV);
        _mdns_append_u16(packet, index, mdns_class);
    } else if (type == MDNS_ANSWER_A) {
        _mdns_append_u16(packet, index, MDNS_TYPE_A);
        _mdns_append_u16(packet, index, mdns_class);
    } else if (type == MDNS_ANSWER_AAAA) {
        _mdns_append_u16(packet, index, MDNS_TYPE_AAAA);
        _mdns_append_u16(packet, index, mdns_class);
    } else {
        return 0;
    }
    _mdns_append_u32(packet, index, ttl);
    _mdns_append_u16(packet, index, 0);
    return 10;
}

static inline uint8_t _mdns_append_string_with_len(uint8_t *packet, uint16_t *index, const char *string, uint8_t len)
{
    if ((*index + len + 1) >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    _mdns_append_u8(packet, index, len);
    memcpy(packet + *index, string, len);
    *index += len;
    return len + 1;
}

/**
 * @brief  appends single string to a packet, incrementing the index
 *
 * @param  packet       MDNS packet
 * @param  index        offset in the packet
 * @param  string       the string to append
 *
 * @return length of added data: 0 on error or length of the string + 1 on success
 */
static inline uint8_t _mdns_append_string(uint8_t *packet, uint16_t *index, const char *string)
{
    uint8_t len = strlen(string);
    if ((*index + len + 1) >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    _mdns_append_u8(packet, index, len);
    memcpy(packet + *index, string, len);
    *index += len;
    return len + 1;
}

/**
 * @brief  appends one TXT record ("key=value" or "key")
 *
 * @param  packet       MDNS packet
 * @param  index        offset in the packet
 * @param  txt          one txt record
 *
 * @return length of added data: length of the added txt value + 1 on success
 *         0  if data won't fit the packet
 *         -1 if invalid TXT entry
 */
static inline int append_one_txt_record_entry(uint8_t *packet, uint16_t *index, mdns_txt_linked_item_t *txt)
{
    if (txt == NULL || txt->key == NULL) {
        return -1;
    }
    size_t key_len = strlen(txt->key);
    size_t len = key_len + txt->value_len + (txt->value ? 1 : 0);
    if ((*index + len + 1) >= MDNS_MAX_PACKET_SIZE) {
        return 0;
    }
    _mdns_append_u8(packet, index, len);
    memcpy(packet + *index, txt->key, key_len);
    if (txt->value) {
        packet[*index + key_len] = '=';
        memcpy(packet + *index + key_len + 1, txt->value, txt->value_len);
    }
    *index += len;
    return len + 1;
}


///
mdns_if_t _mdns_get_other_if(mdns_if_t tcpip_if);
void _mdns_dup_interface(mdns_if_t tcpip_if);

esp_err_t _mdns_sync_browse_action(mdns_action_type_t type, mdns_browse_sync_t *browse_sync);
