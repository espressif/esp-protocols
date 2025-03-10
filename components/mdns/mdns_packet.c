
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns.h"
#include "mdns_mem_caps.h"
#include "esp_log.h"
#include "mdns_utils.h"
#include "mdns_debug.h"
#include "mdns_netif.h"
#include "mdns_send.h"
#include "mdns_browse.h"
#include "mdns_querier.h"
#include "mdns_responder.h"

static const char *TAG = "mdns_packet";

/**
 * @brief  Check if parsed name is discovery
 */
static bool _mdns_name_is_discovery(mdns_name_t *name, uint16_t type)
{
    return (
               (name->host[0] && !strcasecmp(name->host, "_services"))
               && (name->service[0] && !strcasecmp(name->service, "_dns-sd"))
               && (name->proto[0] && !strcasecmp(name->proto, "_udp"))
               && (name->domain[0] && !strcasecmp(name->domain, MDNS_UTILS_DEFAULT_DOMAIN))
               && type == MDNS_TYPE_PTR
           );
}

static mdns_srv_item_t *_mdns_get_service_item_subtype(const char *subtype, const char *service, const char *proto)
{
    mdns_srv_item_t *s = mdns_utils_get_services();
    while (s) {
        if (_mdns_service_match(s->service, service, proto, NULL)) {
            mdns_subtype_t *subtype_item = s->service->subtype;
            while (subtype_item) {
                if (!strcasecmp(subtype_item->subtype, subtype)) {
                    return s;
                }
                subtype_item = subtype_item->next;
            }
        }
        s = s->next;
    }
    return NULL;
}

/**
 * @brief  Check if the parsed name is ours (matches service or host name)
 */
static bool _mdns_name_is_ours(mdns_name_t *name)
{
    //domain have to be "local"
    if (mdns_utils_str_null_or_empty(name->domain) || (strcasecmp(name->domain, MDNS_UTILS_DEFAULT_DOMAIN)
#ifdef CONFIG_MDNS_RESPOND_REVERSE_QUERIES
                                                       && strcasecmp(name->domain, "arpa")
#endif /* CONFIG_MDNS_RESPOND_REVERSE_QUERIES */
                                                      )) {
        return false;
    }

    //if service and proto are empty, host must match out hostname
    if (mdns_utils_str_null_or_empty(name->service) && mdns_utils_str_null_or_empty(name->proto)) {
        if (!mdns_utils_str_null_or_empty(name->host)
                && !mdns_utils_str_null_or_empty(mdns_utils_get_global_hostname())
                && _hostname_is_ours(name->host)) {
            return true;
        }
        return false;
    }

    //if service or proto is empty, name is invalid
    if (mdns_utils_str_null_or_empty(name->service) || mdns_utils_str_null_or_empty(name->proto)) {
        return false;
    }


    //find the service
    mdns_srv_item_t *service;
    if (name->sub) {
        service = _mdns_get_service_item_subtype(name->host, name->service, name->proto);
    } else if (mdns_utils_str_null_or_empty(name->host)) {
        service = _mdns_get_service_item(name->service, name->proto, NULL);
    } else {
        service = _mdns_get_service_item_instance(name->host, name->service, name->proto, NULL);
    }
    if (!service) {
        return false;
    }

    //if query is PTR query and we have service, we have success
    if (name->sub || mdns_utils_str_null_or_empty(name->host)) {
        return true;
    }

    //OK we have host in the name. find what is the instance of the service
    const char *instance = _mdns_get_service_instance_name(service->service);
    if (instance == NULL) {
        return false;
    }

    //compare the instance against the name
    if (strcasecmp(name->host, instance) == 0) {
        return true;
    }

    return false;
}

/**
 * @brief  Duplicate string or return error
 */
static esp_err_t _mdns_strdup_check(char **out, char *in)
{
    if (in && in[0]) {
        *out = mdns_mem_strdup(in);
        if (!*out) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    *out = NULL;
    return ESP_OK;
}

/*
 * @brief  Appends/increments a number to name/instance in case of collision
 * */
static char *_mdns_mangle_name(char *in)
{
    char *p = strrchr(in, '-');
    int suffix = 0;
    if (p == NULL) {
        //No - in ``in``
        suffix = 2;
    } else {
        char *endp = NULL;
        suffix = strtol(p + 1, &endp, 10);
        if (*endp != 0) {
            //suffix is not numerical
            suffix = 2;
            p = NULL; //so we append -suffix to the entire string
        }
    }
    char *ret;
    if (p == NULL) {
        //need to add -2 to string
        ret = mdns_mem_malloc(strlen(in) + 3);
        if (ret == NULL) {
            HOOK_MALLOC_FAILED;
            return NULL;
        }
        sprintf(ret, "%s-2", in);
    } else {
        size_t in_len = strlen(in);
        ret = mdns_mem_malloc(in_len + 2); //one extra byte in case 9-10 or 99-100 etc
        if (ret == NULL) {
            HOOK_MALLOC_FAILED;
            return NULL;
        }
        memcpy(ret, in, in_len);
        int baseLen = p - in; //length of 'bla' in 'bla-123'
        //overwrite suffix with new suffix
        sprintf(ret + baseLen, "-%d", suffix + 1);
    }
    return ret;
}

/**
 * @brief  Get number of items in TXT parsed data
 */
static int _mdns_txt_items_count_get(const uint8_t *data, size_t len)
{
    if (len == 1) {
        return 0;
    }

    int num_items = 0;
    uint16_t i = 0;
    size_t partLen = 0;

    while (i < len) {
        partLen = data[i++];
        if (!partLen) {
            break;
        }
        if ((i + partLen) > len) {
            return -1;//error
        }
        i += partLen;
        num_items++;
    }
    return num_items;
}

/**
 * @brief  Get the length of TXT item's key name
 */
static int _mdns_txt_item_name_get_len(const uint8_t *data, size_t len)
{
    if (*data == '=') {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '=') {
            return i;
        }
    }
    return len;
}



/**
 * @brief  Called from parser to add TXT data to search result
 */
static void _mdns_search_result_add_txt(mdns_search_once_t *search, mdns_txt_item_t *txt, uint8_t *txt_value_len,
                                        size_t txt_count, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol,
                                        uint32_t ttl)
{
    mdns_result_t *r = search->result;
    while (r) {
        if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol) {
            if (r->txt) {
                goto free_txt;
            }
            r->txt = txt;
            r->txt_value_len = txt_value_len;
            r->txt_count = txt_count;
            _mdns_result_update_ttl(r, ttl);
            return;
        }
        r = r->next;
    }
    if (!search->max_results || search->num_results < search->max_results) {
        r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
        if (!r) {
            HOOK_MALLOC_FAILED;
            goto free_txt;
        }

        memset(r, 0, sizeof(mdns_result_t));
        r->txt = txt;
        r->txt_value_len = txt_value_len;
        r->txt_count = txt_count;
        r->esp_netif = _mdns_get_esp_netif(tcpip_if);
        r->ip_protocol = ip_protocol;
        r->ttl = ttl;
        r->next = search->result;
        search->result = r;
        search->num_results++;
    }
    return;

free_txt:
    for (size_t i = 0; i < txt_count; i++) {
        mdns_mem_free((char *)(txt[i].key));
        mdns_mem_free((char *)(txt[i].value));
    }
    mdns_mem_free(txt);
    mdns_mem_free(txt_value_len);
}


/**
 * @brief  Create TXT result array from parsed TXT data
 */
static void _mdns_result_txt_create(const uint8_t *data, size_t len, mdns_txt_item_t **out_txt, uint8_t **out_value_len,
                                    size_t *out_count)
{
    *out_txt = NULL;
    *out_count = 0;
    uint16_t i = 0, y;
    size_t partLen = 0;
    int num_items = _mdns_txt_items_count_get(data, len);
    if (num_items < 0 || num_items > SIZE_MAX / sizeof(mdns_txt_item_t)) {
        // Error: num_items is incorrect (or too large to allocate)
        return;
    }

    if (!num_items) {
        return;
    }

    mdns_txt_item_t *txt = (mdns_txt_item_t *)mdns_mem_malloc(sizeof(mdns_txt_item_t) * num_items);
    if (!txt) {
        HOOK_MALLOC_FAILED;
        return;
    }
    uint8_t *txt_value_len = (uint8_t *)mdns_mem_malloc(num_items);
    if (!txt_value_len) {
        mdns_mem_free(txt);
        HOOK_MALLOC_FAILED;
        return;
    }
    memset(txt, 0, sizeof(mdns_txt_item_t) * num_items);
    memset(txt_value_len, 0, num_items);
    size_t txt_num = 0;

    while (i < len) {
        partLen = data[i++];
        if (!partLen) {
            break;
        }

        if ((i + partLen) > len) {
            goto handle_error;//error
        }

        int name_len = _mdns_txt_item_name_get_len(data + i, partLen);
        if (name_len < 0) {//invalid item (no name)
            i += partLen;
            continue;
        }
        char *key = (char *)mdns_mem_malloc(name_len + 1);
        if (!key) {
            HOOK_MALLOC_FAILED;
            goto handle_error;//error
        }

        mdns_txt_item_t *t = &txt[txt_num];
        uint8_t *value_len = &txt_value_len[txt_num];
        txt_num++;

        memcpy(key, data + i, name_len);
        key[name_len] = 0;
        i += name_len + 1;
        t->key = key;

        int new_value_len = partLen - name_len - 1;
        if (new_value_len > 0) {
            char *value = (char *)mdns_mem_malloc(new_value_len + 1);
            if (!value) {
                HOOK_MALLOC_FAILED;
                goto handle_error;//error
            }
            memcpy(value, data + i, new_value_len);
            value[new_value_len] = 0;
            *value_len = new_value_len;
            i += new_value_len;
            t->value = value;
        }
    }

    *out_txt = txt;
    *out_count = txt_num;
    *out_value_len = txt_value_len;
    return;

handle_error :
    for (y = 0; y < txt_num; y++) {
        mdns_txt_item_t *t = &txt[y];
        mdns_mem_free((char *)t->key);
        mdns_mem_free((char *)t->value);
    }
    mdns_mem_free(txt_value_len);
    mdns_mem_free(txt);
}

static bool is_txt_item_in_list(mdns_txt_item_t txt, uint8_t txt_value_len, mdns_txt_item_t *txt_list, uint8_t *txt_value_len_list, size_t txt_count)
{
    for (size_t i = 0; i < txt_count; i++) {
        if (strcmp(txt.key, txt_list[i].key) == 0) {
            if (txt_value_len == txt_value_len_list[i] && memcmp(txt.value, txt_list[i].value, txt_value_len) == 0) {
                return true;
            } else {
                // The key value is unique, so there is no need to continue searching.
                return false;
            }
        }
    }
    return false;
}

/**
 * @brief  Create linked IP (copy) from parsed one
 */
static mdns_ip_addr_t *_mdns_result_addr_create_ip(esp_ip_addr_t *ip)
{
    mdns_ip_addr_t *a = (mdns_ip_addr_t *)mdns_mem_malloc(sizeof(mdns_ip_addr_t));
    if (!a) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }
    memset(a, 0, sizeof(mdns_ip_addr_t));
    a->addr.type = ip->type;
    if (ip->type == ESP_IPADDR_TYPE_V6) {
        memcpy(a->addr.u_addr.ip6.addr, ip->u_addr.ip6.addr, 16);
    } else {
        a->addr.u_addr.ip4.addr = ip->u_addr.ip4.addr;
    }
    return a;
}

/**
 * @brief  Chain new IP to search result
 */
static void _mdns_result_add_ip(mdns_result_t *r, esp_ip_addr_t *ip)
{
    mdns_ip_addr_t *a = r->addr;
    while (a) {
        if (a->addr.type == ip->type) {
#ifdef CONFIG_LWIP_IPV4
            if (a->addr.type == ESP_IPADDR_TYPE_V4 && a->addr.u_addr.ip4.addr == ip->u_addr.ip4.addr) {
                return;
            }
#endif
#ifdef CONFIG_LWIP_IPV6
            if (a->addr.type == ESP_IPADDR_TYPE_V6 && !memcmp(a->addr.u_addr.ip6.addr, ip->u_addr.ip6.addr, 16)) {
                return;
            }
#endif
        }
        a = a->next;
    }
    a = _mdns_result_addr_create_ip(ip);
    if (!a) {
        return;
    }
    a->next = r->addr;
    r->addr = a;
}

/**
 * @brief  Called from parser to add A/AAAA data to search result
 */
static void _mdns_search_result_add_ip(mdns_search_once_t *search, const char *hostname, esp_ip_addr_t *ip,
                                       mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl)
{
    mdns_result_t *r = NULL;
    mdns_ip_addr_t *a = NULL;

    if ((search->type == MDNS_TYPE_A && ip->type == ESP_IPADDR_TYPE_V4)
            || (search->type == MDNS_TYPE_AAAA && ip->type == ESP_IPADDR_TYPE_V6)
            || search->type == MDNS_TYPE_ANY) {
        r = search->result;
        while (r) {
            if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol) {
                _mdns_result_add_ip(r, ip);
                _mdns_result_update_ttl(r, ttl);
                return;
            }
            r = r->next;
        }
        if (!search->max_results || search->num_results < search->max_results) {
            r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
            if (!r) {
                HOOK_MALLOC_FAILED;
                return;
            }

            memset(r, 0, sizeof(mdns_result_t));

            a = _mdns_result_addr_create_ip(ip);
            if (!a) {
                mdns_mem_free(r);
                return;
            }
            a->next = r->addr;
            r->hostname = mdns_mem_strdup(hostname);
            r->addr = a;
            r->esp_netif = _mdns_get_esp_netif(tcpip_if);
            r->ip_protocol = ip_protocol;
            r->next = search->result;
            r->ttl = ttl;
            search->result = r;
            search->num_results++;
        }
    } else if (search->type == MDNS_TYPE_PTR || search->type == MDNS_TYPE_SRV) {
        r = search->result;
        while (r) {
            if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(hostname, r->hostname)) {
                _mdns_result_add_ip(r, ip);
                _mdns_result_update_ttl(r, ttl);
                break;
            }
            r = r->next;
        }
    }
}

/**
 * @brief  Called from parser to add A/AAAA data to search result
 */
static void _mdns_browse_result_add_ip(mdns_browse_t *browse, const char *hostname, esp_ip_addr_t *ip,
                                       mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse)
{
    if (out_sync_browse->browse == NULL) {
        return;
    } else {
        if (out_sync_browse->browse != browse) {
            return;
        }
    }
    mdns_result_t *r = NULL;
    mdns_ip_addr_t *r_a = NULL;
    if (browse) {
        r = browse->result;
        while (r) {
            if (r->ip_protocol == ip_protocol) {
                // Find the target result in browse result.
                if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(hostname, r->hostname)) {
                    r_a = r->addr;
                    // Check if the address has already added in result.
                    while (r_a) {
#ifdef CONFIG_LWIP_IPV4
                        if (r_a->addr.type == ip->type && r_a->addr.type == ESP_IPADDR_TYPE_V4 && r_a->addr.u_addr.ip4.addr == ip->u_addr.ip4.addr) {
                            break;
                        }
#endif
#ifdef CONFIG_LWIP_IPV6
                        if (r_a->addr.type == ip->type && r_a->addr.type == ESP_IPADDR_TYPE_V6 && !memcmp(r_a->addr.u_addr.ip6.addr, ip->u_addr.ip6.addr, 16)) {
                            break;
                        }
#endif
                        r_a = r_a->next;
                    }
                    if (!r_a) {
                        // The current IP is a new one, add it to the link list.
                        mdns_ip_addr_t *a = NULL;
                        a = _mdns_result_addr_create_ip(ip);
                        if (!a) {
                            return;
                        }
                        a->next = r->addr;
                        r->addr = a;
                        if (r->ttl != ttl) {
                            if (r->ttl == 0) {
                                r->ttl = ttl;
                            } else {
                                _mdns_result_update_ttl(r, ttl);
                            }
                        }
                        if (_mdns_add_browse_result(out_sync_browse, r) != ESP_OK) {
                            return;
                        }
                        break;
                    }
                }
            }
            r = r->next;
        }
    }
    return;
}


/**
 * @brief  Called from parser to add TXT data to search result
 */
static void _mdns_browse_result_add_txt(mdns_browse_t *browse, const char *instance, const char *service, const char *proto,
                                        mdns_txt_item_t *txt, uint8_t *txt_value_len, size_t txt_count, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol,
                                        uint32_t ttl, mdns_browse_sync_t *out_sync_browse)
{
    if (out_sync_browse->browse == NULL) {
        return;
    } else {
        if (out_sync_browse->browse != browse) {
            return;
        }
    }
    mdns_result_t *r = browse->result;
    while (r) {
        if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol &&
                !mdns_utils_str_null_or_empty(r->instance_name) && !strcasecmp(instance, r->instance_name) &&
                !mdns_utils_str_null_or_empty(r->service_type) && !strcasecmp(service, r->service_type) &&
                !mdns_utils_str_null_or_empty(r->proto) && !strcasecmp(proto, r->proto)) {
            bool should_update = false;
            if (r->txt) {
                // Check if txt changed
                if (txt_count != r->txt_count) {
                    should_update = true;
                } else {
                    for (size_t txt_index = 0; txt_index < txt_count; txt_index++) {
                        if (!is_txt_item_in_list(txt[txt_index], txt_value_len[txt_index], r->txt, r->txt_value_len, r->txt_count)) {
                            should_update = true;
                            break;
                        }
                    }
                }
                // If the result has a previous txt entry, we delete it and re-add.
                for (size_t i = 0; i < r->txt_count; i++) {
                    mdns_mem_free((char *)(r->txt[i].key));
                    mdns_mem_free((char *)(r->txt[i].value));
                }
                mdns_mem_free(r->txt);
                mdns_mem_free(r->txt_value_len);
            }
            r->txt = txt;
            r->txt_value_len = txt_value_len;
            r->txt_count = txt_count;
            if (r->ttl != ttl) {
                uint32_t previous_ttl = r->ttl;
                if (r->ttl == 0) {
                    r->ttl = ttl;
                } else {
                    _mdns_result_update_ttl(r, ttl);
                }
                if (previous_ttl != r->ttl) {
                    should_update = true;
                }
            }
            if (should_update) {
                if (_mdns_add_browse_result(out_sync_browse, r) != ESP_OK) {
                    return;
                }
            }
            return;
        }
        r = r->next;
    }
    r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
    if (!r) {
        HOOK_MALLOC_FAILED;
        goto free_txt;
    }
    memset(r, 0, sizeof(mdns_result_t));
    r->instance_name = mdns_mem_strdup(instance);
    r->service_type = mdns_mem_strdup(service);
    r->proto = mdns_mem_strdup(proto);
    if (!r->instance_name || !r->service_type || !r->proto) {
        mdns_mem_free(r->instance_name);
        mdns_mem_free(r->service_type);
        mdns_mem_free(r->proto);
        mdns_mem_free(r);
        return;
    }
    r->txt = txt;
    r->txt_value_len = txt_value_len;
    r->txt_count = txt_count;
    r->esp_netif = _mdns_get_esp_netif(tcpip_if);
    r->ip_protocol = ip_protocol;
    r->ttl = ttl;
    r->next = browse->result;
    browse->result = r;
    _mdns_add_browse_result(out_sync_browse, r);
    return;

free_txt:
    for (size_t i = 0; i < txt_count; i++) {
        mdns_mem_free((char *)(txt[i].key));
        mdns_mem_free((char *)(txt[i].value));
    }
    mdns_mem_free(txt);
    mdns_mem_free(txt_value_len);
    return;
}

#ifdef CONFIG_LWIP_IPV4
/**
 * @brief  Detect IPv4 address collision
 */
static int _mdns_check_a_collision(esp_ip4_addr_t *ip, mdns_if_t tcpip_if)
{
    esp_netif_ip_info_t if_ip_info;
    esp_netif_ip_info_t other_ip_info;
    if (!ip->addr) {
        return 1;//denial! they win
    }
    if (esp_netif_get_ip_info(_mdns_get_esp_netif(tcpip_if), &if_ip_info)) {
        return 1;//they win
    }
    int ret = memcmp((uint8_t *)&if_ip_info.ip.addr, (uint8_t *)&ip->addr, sizeof(esp_ip4_addr_t));
    if (ret > 0) {
        return -1;//we win
    } else if (ret < 0) {
        //is it the other interface?
        mdns_if_t other_if = _mdns_get_other_if(tcpip_if);
        if (other_if == MDNS_MAX_INTERFACES) {
            return 1;//AP interface! They win
        }
        if (esp_netif_get_ip_info(_mdns_get_esp_netif(other_if), &other_ip_info)) {
            return 1;//IPv4 not active! They win
        }
        if (ip->addr != other_ip_info.ip.addr) {
            return 1;//IPv4 not ours! They win
        }
        _mdns_dup_interface(tcpip_if);
        return 2;//they win
    }
    return 0;//same
}
#endif /* CONFIG_LWIP_IPV4 */

#ifdef CONFIG_LWIP_IPV6
/**
 * @brief  Detect IPv6 address collision
 */
static int _mdns_check_aaaa_collision(esp_ip6_addr_t *ip, mdns_if_t tcpip_if)
{
    struct esp_ip6_addr if_ip6;
    struct esp_ip6_addr other_ip6;
    if (_ipv6_address_is_zero(*ip)) {
        return 1;//denial! they win
    }
    if (esp_netif_get_ip6_linklocal(_mdns_get_esp_netif(tcpip_if), &if_ip6)) {
        return 1;//they win
    }
    int ret = memcmp((uint8_t *)&if_ip6.addr, (uint8_t *)ip->addr, _MDNS_SIZEOF_IP6_ADDR);
    if (ret > 0) {
        return -1;//we win
    } else if (ret < 0) {
        //is it the other interface?
        mdns_if_t other_if = _mdns_get_other_if(tcpip_if);
        if (other_if == MDNS_MAX_INTERFACES) {
            return 1;//AP interface! They win
        }
        if (esp_netif_get_ip6_linklocal(_mdns_get_esp_netif(other_if), &other_ip6)) {
            return 1;//IPv6 not active! They win
        }
        if (memcmp((uint8_t *)&other_ip6.addr, (uint8_t *)ip->addr, _MDNS_SIZEOF_IP6_ADDR)) {
            return 1;//IPv6 not ours! They win
        }
        _mdns_dup_interface(tcpip_if);
        return 2;//they win
    }
    return 0;//same
}
#endif /* CONFIG_LWIP_IPV6 */


/**
 * @brief  Detect TXT collision
 */
static int _mdns_check_txt_collision(mdns_service_t *service, const uint8_t *data, size_t len)
{
    size_t data_len = 0;
    if (len <= 1 && service->txt) {     // len==0 means incorrect packet (and handled by the packet parser)
        // but handled here again to fix clang-tidy warning on VLA "uint8_t our[0];"
        return -1;//we win
    } else if (len > 1 && !service->txt) {
        return 1;//they win
    } else if (len <= 1 && !service->txt) {
        return 0;//same
    }

    mdns_txt_linked_item_t *txt = service->txt;
    while (txt) {
        data_len += 1 /* record-len */ + strlen(txt->key) + txt->value_len + (txt->value ? 1 : 0 /* "=" */);
        txt = txt->next;
    }

    if (len > data_len) {
        return 1;//they win
    } else if (len < data_len) {
        return -1;//we win
    }

    uint8_t ours[len];
    uint16_t index = 0;

    txt = service->txt;
    while (txt) {
        append_one_txt_record_entry(ours, &index, txt);
        txt = txt->next;
    }

    int ret = memcmp(ours, data, len);
    if (ret > 0) {
        return -1;//we win
    } else if (ret < 0) {
        return 1;//they win
    }
    return 0;//same
}


/**
 * @brief  Detect SRV collision
 */
static int _mdns_check_srv_collision(mdns_service_t *service, uint16_t priority, uint16_t weight, uint16_t port, const char *host, const char *domain)
{
    if (mdns_utils_str_null_or_empty(mdns_utils_get_global_hostname())) {
        return 0;
    }

    size_t our_host_len = strlen(mdns_utils_get_global_hostname());
    size_t our_len = 14 + our_host_len;

    size_t their_host_len = strlen(host);
    size_t their_domain_len = strlen(domain);
    size_t their_len = 9 + their_host_len + their_domain_len;

    if (their_len > our_len) {
        return 1;//they win
    } else if (their_len < our_len) {
        return -1;//we win
    }

    uint16_t our_index = 0;
    uint8_t our_data[our_len];
    _mdns_append_u16(our_data, &our_index, service->priority);
    _mdns_append_u16(our_data, &our_index, service->weight);
    _mdns_append_u16(our_data, &our_index, service->port);
    our_data[our_index++] = our_host_len;
    memcpy(our_data + our_index, mdns_utils_get_global_hostname(), our_host_len);
    our_index += our_host_len;
    our_data[our_index++] = 5;
    memcpy(our_data + our_index, MDNS_UTILS_DEFAULT_DOMAIN, 5);
    our_index += 5;
    our_data[our_index++] = 0;

    uint16_t their_index = 0;
    uint8_t their_data[their_len];
    _mdns_append_u16(their_data, &their_index, priority);
    _mdns_append_u16(their_data, &their_index, weight);
    _mdns_append_u16(their_data, &their_index, port);
    their_data[their_index++] = their_host_len;
    memcpy(their_data + their_index, host, their_host_len);
    their_index += their_host_len;
    their_data[their_index++] = their_domain_len;
    memcpy(their_data + their_index, domain, their_domain_len);
    their_index += their_domain_len;
    their_data[their_index++] = 0;

    int ret = memcmp(our_data, their_data, our_len);
    if (ret > 0) {
        return -1;//we win
    } else if (ret < 0) {
        return 1;//they win
    }
    return 0;//same
}

/**
 * @brief  Called from parser to add SRV data to search result
 */
static void _mdns_search_result_add_srv(mdns_search_once_t *search, const char *hostname, uint16_t port,
                                        mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl)
{
    mdns_result_t *r = search->result;
    while (r) {
        if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(hostname, r->hostname)) {
            _mdns_result_update_ttl(r, ttl);
            return;
        }
        r = r->next;
    }
    if (!search->max_results || search->num_results < search->max_results) {
        r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
        if (!r) {
            HOOK_MALLOC_FAILED;
            return;
        }

        memset(r, 0, sizeof(mdns_result_t));
        r->hostname = mdns_mem_strdup(hostname);
        if (!r->hostname) {
            mdns_mem_free(r);
            return;
        }
        if (search->instance) {
            r->instance_name = mdns_mem_strdup(search->instance);
        }
        r->service_type = mdns_mem_strdup(search->service);
        r->proto = mdns_mem_strdup(search->proto);
        r->port = port;
        r->esp_netif = _mdns_get_esp_netif(tcpip_if);
        r->ip_protocol = ip_protocol;
        r->ttl = ttl;
        r->next = search->result;
        search->result = r;
        search->num_results++;
    }
}

static esp_err_t _mdns_copy_address_in_previous_result(mdns_result_t *result_list, mdns_result_t *r)
{
    while (result_list) {
        if (!mdns_utils_str_null_or_empty(result_list->hostname) && !mdns_utils_str_null_or_empty(r->hostname) && !strcasecmp(result_list->hostname, r->hostname) &&
                result_list->ip_protocol == r->ip_protocol && result_list->addr && !r->addr) {
            // If there is a same hostname in previous result, we need to copy the address here.
            r->addr = copy_address_list(result_list->addr);
            if (!r->addr) {
                return ESP_ERR_NO_MEM;
            }
            break;
        } else {
            result_list = result_list->next;
        }
    }
    return ESP_OK;
}

/**
 * @brief  Called from parser to add SRV data to search result
 */
static void _mdns_browse_result_add_srv(mdns_browse_t *browse, const char *hostname, const char *instance, const char *service, const char *proto,
                                        uint16_t port, mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, uint32_t ttl, mdns_browse_sync_t *out_sync_browse)
{
    if (out_sync_browse->browse == NULL) {
        return;
    } else {
        if (out_sync_browse->browse != browse) {
            return;
        }
    }
    mdns_result_t *r = browse->result;
    while (r) {
        if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol &&
                !mdns_utils_str_null_or_empty(r->instance_name) && !strcasecmp(instance, r->instance_name) &&
                !mdns_utils_str_null_or_empty(r->service_type) && !strcasecmp(service, r->service_type) &&
                !mdns_utils_str_null_or_empty(r->proto) && !strcasecmp(proto, r->proto)) {
            if (mdns_utils_str_null_or_empty(r->hostname) || strcasecmp(hostname, r->hostname)) {
                r->hostname = mdns_mem_strdup(hostname);
                r->port = port;
                if (!r->hostname) {
                    HOOK_MALLOC_FAILED;
                    return;
                }
                if (!r->addr) {
                    esp_err_t err = _mdns_copy_address_in_previous_result(browse->result, r);
                    if (err == ESP_ERR_NO_MEM) {
                        return;
                    }
                }
                if (_mdns_add_browse_result(out_sync_browse, r) != ESP_OK) {
                    return;
                }
            }
            if (r->ttl != ttl) {
                uint32_t previous_ttl = r->ttl;
                if (r->ttl == 0) {
                    r->ttl = ttl;
                } else {
                    _mdns_result_update_ttl(r, ttl);
                }
                if (previous_ttl != r->ttl) {
                    if (_mdns_add_browse_result(out_sync_browse, r) != ESP_OK) {
                        return;
                    }
                }
            }
            return;
        }
        r = r->next;
    }
    r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
    if (!r) {
        HOOK_MALLOC_FAILED;
        return;
    }

    memset(r, 0, sizeof(mdns_result_t));
    r->hostname = mdns_mem_strdup(hostname);
    r->instance_name = mdns_mem_strdup(instance);
    r->service_type = mdns_mem_strdup(service);
    r->proto = mdns_mem_strdup(proto);
    if (!r->hostname || !r->instance_name || !r->service_type || !r->proto) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(r->hostname);
        mdns_mem_free(r->instance_name);
        mdns_mem_free(r->service_type);
        mdns_mem_free(r->proto);
        mdns_mem_free(r);
        return;
    }
    r->port = port;
    r->esp_netif = _mdns_get_esp_netif(tcpip_if);
    r->ip_protocol = ip_protocol;
    r->ttl = ttl;
    r->next = browse->result;
    browse->result = r;
    _mdns_add_browse_result(out_sync_browse, r);
    return;
}


/**
 * @brief  Check if the parsed name is self-hosted, i.e. we should resolve conflicts
 */
static bool _mdns_name_is_selfhosted(mdns_name_t *name)
{
    if (mdns_utils_str_null_or_empty(mdns_utils_get_global_hostname())) { // self-hostname needs to be defined
        return false;
    }

    // hostname only -- check if selfhosted name
    if (mdns_utils_str_null_or_empty(name->service) && mdns_utils_str_null_or_empty(name->proto) &&
            strcasecmp(name->host, mdns_utils_get_global_hostname()) == 0) {
        return true;
    }

    // service -- check if selfhosted service
    mdns_srv_item_t *srv = _mdns_get_service_item(name->service, name->proto, NULL);
    if (srv && strcasecmp(mdns_utils_get_global_hostname(), srv->service->hostname) == 0) {
        return true;
    }
    return false;
}





/**
 * @brief  Called from parser to check if question matches particular service
 */
static bool _mdns_question_matches(mdns_parsed_question_t *question, uint16_t type, mdns_srv_item_t *service)
{
    if (question->type != type) {
        return false;
    }
    if (type == MDNS_TYPE_A || type == MDNS_TYPE_AAAA) {
        return true;
    } else if (type == MDNS_TYPE_PTR || type == MDNS_TYPE_SDPTR) {
        if (question->service && question->proto && question->domain
                && !strcasecmp(service->service->service, question->service)
                && !strcasecmp(service->service->proto, question->proto)
                && !strcasecmp(MDNS_UTILS_DEFAULT_DOMAIN, question->domain)) {
            if (!service->service->instance) {
                return true;
            } else if (service->service->instance && question->host && !strcasecmp(service->service->instance, question->host)) {
                return true;
            }
        }
    } else if (service && (type == MDNS_TYPE_SRV || type == MDNS_TYPE_TXT)) {
        const char *name = _mdns_get_service_instance_name(service->service);
        if (name && question->host && question->service && question->proto && question->domain
                && !strcasecmp(name, question->host)
                && !strcasecmp(service->service->service, question->service)
                && !strcasecmp(service->service->proto, question->proto)
                && !strcasecmp(MDNS_UTILS_DEFAULT_DOMAIN, question->domain)) {
            return true;
        }
    }

    return false;
}

/**
 * @brief  Removes saved question from parsed data
 */
static void _mdns_remove_parsed_question(mdns_parsed_packet_t *parsed_packet, uint16_t type, mdns_srv_item_t *service)
{
    mdns_parsed_question_t *q = parsed_packet->questions;

    if (_mdns_question_matches(q, type, service)) {
        parsed_packet->questions = q->next;
        mdns_mem_free(q->host);
        mdns_mem_free(q->service);
        mdns_mem_free(q->proto);
        mdns_mem_free(q->domain);
        mdns_mem_free(q);
        return;
    }

    while (q->next) {
        mdns_parsed_question_t *p = q->next;
        if (_mdns_question_matches(p, type, service)) {
            q->next = p->next;
            mdns_mem_free(p->host);
            mdns_mem_free(p->service);
            mdns_mem_free(p->proto);
            mdns_mem_free(p->domain);
            mdns_mem_free(p);
            return;
        }
        q = q->next;
    }
}


/**
 * @brief  Called from parser to add PTR data to search result
 */
static mdns_result_t *_mdns_search_result_add_ptr(mdns_search_once_t *search, const char *instance,
                                                  const char *service_type, const char *proto, mdns_if_t tcpip_if,
                                                  mdns_ip_protocol_t ip_protocol, uint32_t ttl)
{
    mdns_result_t *r = search->result;
    while (r) {
        if (r->esp_netif == _mdns_get_esp_netif(tcpip_if) && r->ip_protocol == ip_protocol && !mdns_utils_str_null_or_empty(r->instance_name) && !strcasecmp(instance, r->instance_name)) {
            _mdns_result_update_ttl(r, ttl);
            return r;
        }
        r = r->next;
    }
    if (!search->max_results || search->num_results < search->max_results) {
        r = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
        if (!r) {
            HOOK_MALLOC_FAILED;
            return NULL;
        }

        memset(r, 0, sizeof(mdns_result_t));
        r->instance_name = mdns_mem_strdup(instance);
        r->service_type = mdns_mem_strdup(service_type);
        r->proto = mdns_mem_strdup(proto);
        if (!r->instance_name) {
            mdns_mem_free(r);
            return NULL;
        }

        r->esp_netif = _mdns_get_esp_netif(tcpip_if);
        r->ip_protocol = ip_protocol;
        r->ttl = ttl;
        r->next = search->result;
        search->result = r;
        search->num_results++;
        return r;
    }
    return NULL;
}




/**
 * @brief  main packet parser
 *
 * @param  packet       the packet
 */
void mdns_parse_packet(mdns_rx_packet_t *packet)
{
    static mdns_name_t n;
    mdns_header_t header;
    const uint8_t *data = _mdns_get_packet_data(packet);
    size_t len = _mdns_get_packet_len(packet);
    const uint8_t *content = data + MDNS_HEAD_LEN;
    bool do_not_reply = false;
    mdns_search_once_t *search_result = NULL;
    mdns_browse_t *browse_result = NULL;
    char *browse_result_instance = NULL;
    char *browse_result_service = NULL;
    char *browse_result_proto = NULL;
    mdns_browse_sync_t *out_sync_browse = NULL;

    DBG_RX_PACKET(packet, data, len);

#ifndef CONFIG_MDNS_SKIP_SUPPRESSING_OWN_QUERIES
    // Check if the packet wasn't sent by us
#ifdef CONFIG_LWIP_IPV4
    if (packet->ip_protocol == MDNS_IP_PROTOCOL_V4) {
        esp_netif_ip_info_t if_ip_info;
        if (esp_netif_get_ip_info(_mdns_get_esp_netif(packet->tcpip_if), &if_ip_info) == ESP_OK &&
                memcmp(&if_ip_info.ip.addr, &packet->src.u_addr.ip4.addr, sizeof(esp_ip4_addr_t)) == 0) {
            return;
        }
    }
#endif /* CONFIG_LWIP_IPV4 */
#ifdef CONFIG_LWIP_IPV6
    if (packet->ip_protocol == MDNS_IP_PROTOCOL_V6) {
        struct esp_ip6_addr if_ip6;
        if (esp_netif_get_ip6_linklocal(_mdns_get_esp_netif(packet->tcpip_if), &if_ip6) == ESP_OK &&
                memcmp(&if_ip6, &packet->src.u_addr.ip6, sizeof(esp_ip6_addr_t)) == 0) {
            return;
        }
    }
#endif /* CONFIG_LWIP_IPV6 */
#endif // CONFIG_MDNS_SKIP_SUPPRESSING_OWN_QUERIES

    // Check for the minimum size of mdns packet
    if (len <=  MDNS_HEAD_ADDITIONAL_OFFSET) {
        return;
    }

    mdns_parsed_packet_t *parsed_packet = (mdns_parsed_packet_t *)mdns_mem_malloc(sizeof(mdns_parsed_packet_t));
    if (!parsed_packet) {
        HOOK_MALLOC_FAILED;
        return;
    }
    memset(parsed_packet, 0, sizeof(mdns_parsed_packet_t));

    mdns_name_t *name = &n;
    memset(name, 0, sizeof(mdns_name_t));

    header.id = mdns_utils_read_u16(data, MDNS_HEAD_ID_OFFSET);
    header.flags = mdns_utils_read_u16(data, MDNS_HEAD_FLAGS_OFFSET);
    header.questions = mdns_utils_read_u16(data, MDNS_HEAD_QUESTIONS_OFFSET);
    header.answers = mdns_utils_read_u16(data, MDNS_HEAD_ANSWERS_OFFSET);
    header.servers = mdns_utils_read_u16(data, MDNS_HEAD_SERVERS_OFFSET);
    header.additional = mdns_utils_read_u16(data, MDNS_HEAD_ADDITIONAL_OFFSET);

    if (header.flags == MDNS_FLAGS_QR_AUTHORITATIVE && packet->src_port != MDNS_SERVICE_PORT) {
        mdns_mem_free(parsed_packet);
        return;
    }

    //if we have not set the hostname, we can not answer questions
    if (header.questions && !header.answers && mdns_utils_str_null_or_empty(mdns_utils_get_global_hostname())) {
        mdns_mem_free(parsed_packet);
        return;
    }

    parsed_packet->tcpip_if = packet->tcpip_if;
    parsed_packet->ip_protocol = packet->ip_protocol;
    parsed_packet->multicast = packet->multicast;
    parsed_packet->authoritative = (header.flags == MDNS_FLAGS_QR_AUTHORITATIVE);
    parsed_packet->distributed = header.flags == MDNS_FLAGS_DISTRIBUTED;
    parsed_packet->id = header.id;
    esp_netif_ip_addr_copy(&parsed_packet->src, &packet->src);
    parsed_packet->src_port = packet->src_port;
    parsed_packet->records = NULL;

    if (header.questions) {
        uint8_t qs = header.questions;

        while (qs--) {
            content = _mdns_parse_fqdn(data, content, name, len);
            if (!content) {
                header.answers = 0;
                header.additional = 0;
                header.servers = 0;
                goto clear_rx_packet;//error
            }

            if (content + MDNS_CLASS_OFFSET + 1 >= data + len) {
                goto clear_rx_packet; // malformed packet, won't read behind it
            }
            uint16_t type = mdns_utils_read_u16(content, MDNS_TYPE_OFFSET);
            uint16_t mdns_class = mdns_utils_read_u16(content, MDNS_CLASS_OFFSET);
            bool unicast = !!(mdns_class & 0x8000);
            mdns_class &= 0x7FFF;
            content = content + 4;

            if (mdns_class != 0x0001 || name->invalid) {//bad class or invalid name for this question entry
                continue;
            }

            if (_mdns_name_is_discovery(name, type)) {
                //service discovery
                parsed_packet->discovery = true;
                mdns_srv_item_t *a = mdns_utils_get_services();
                while (a) {
                    mdns_parsed_question_t *question = (mdns_parsed_question_t *)mdns_mem_calloc(1, sizeof(mdns_parsed_question_t));
                    if (!question) {
                        HOOK_MALLOC_FAILED;
                        goto clear_rx_packet;
                    }
                    question->next = parsed_packet->questions;
                    parsed_packet->questions = question;

                    question->unicast = unicast;
                    question->type = MDNS_TYPE_SDPTR;
                    question->host = NULL;
                    question->service = mdns_mem_strdup(a->service->service);
                    question->proto = mdns_mem_strdup(a->service->proto);
                    question->domain = mdns_mem_strdup(MDNS_UTILS_DEFAULT_DOMAIN);
                    if (!question->service || !question->proto || !question->domain) {
                        goto clear_rx_packet;
                    }
                    a = a->next;
                }
                continue;
            }
            if (!_mdns_name_is_ours(name)) {
                continue;
            }

            if (type == MDNS_TYPE_ANY && !mdns_utils_str_null_or_empty(name->host)) {
                parsed_packet->probe = true;
            }

            mdns_parsed_question_t *question = (mdns_parsed_question_t *)mdns_mem_calloc(1, sizeof(mdns_parsed_question_t));
            if (!question) {
                HOOK_MALLOC_FAILED;
                goto clear_rx_packet;
            }
            question->next = parsed_packet->questions;
            parsed_packet->questions = question;

            question->unicast = unicast;
            question->type = type;
            question->sub = name->sub;
            if (_mdns_strdup_check(&(question->host), name->host)
                    || _mdns_strdup_check(&(question->service), name->service)
                    || _mdns_strdup_check(&(question->proto), name->proto)
                    || _mdns_strdup_check(&(question->domain), name->domain)) {
                goto clear_rx_packet;
            }
        }
    }

    if (header.questions && !parsed_packet->questions && !parsed_packet->discovery && !header.answers) {
        goto clear_rx_packet;
    } else if (header.answers || header.servers || header.additional) {
        uint16_t recordIndex = 0;

        while (content < (data + len)) {

            content = _mdns_parse_fqdn(data, content, name, len);
            if (!content) {
                goto clear_rx_packet;//error
            }

            if (content + MDNS_LEN_OFFSET + 1 >= data + len) {
                goto clear_rx_packet; // malformed packet, won't read behind it
            }
            uint16_t type = mdns_utils_read_u16(content, MDNS_TYPE_OFFSET);
            uint16_t mdns_class = mdns_utils_read_u16(content, MDNS_CLASS_OFFSET);
            uint32_t ttl = mdns_utils_read_u32(content, MDNS_TTL_OFFSET);
            uint16_t data_len = mdns_utils_read_u16(content, MDNS_LEN_OFFSET);
            const uint8_t *data_ptr = content + MDNS_DATA_OFFSET;
            mdns_class &= 0x7FFF;

            content = data_ptr + data_len;
            if (content > (data + len) || data_len == 0) {
                goto clear_rx_packet;
            }

            bool discovery = false;
            bool ours = false;
            mdns_srv_item_t *service = NULL;
            mdns_parsed_record_type_t record_type = MDNS_ANSWER;

            if (recordIndex >= (header.answers + header.servers)) {
                record_type = MDNS_EXTRA;
            } else if (recordIndex >= (header.answers)) {
                record_type = MDNS_NS;
            }
            recordIndex++;

            if (type == MDNS_TYPE_NSEC || type == MDNS_TYPE_OPT) {
                //skip NSEC and OPT
                continue;
            }

            if (parsed_packet->discovery && _mdns_name_is_discovery(name, type)) {
                discovery = true;
            } else if (!name->sub && _mdns_name_is_ours(name)) {
                ours = true;
                if (name->service[0] && name->proto[0]) {
                    service = _mdns_get_service_item(name->service, name->proto, NULL);
                }
            } else {
                if ((header.flags & MDNS_FLAGS_QUERY_REPSONSE) == 0 || record_type == MDNS_NS) {
                    //skip this record
                    continue;
                }
                search_result = _mdns_search_find(name, type, packet->tcpip_if, packet->ip_protocol);
                browse_result = _mdns_browse_find(name, type, packet->tcpip_if, packet->ip_protocol);
                if (browse_result) {
                    if (!out_sync_browse) {
                        // will be freed in function `_mdns_browse_sync`
                        out_sync_browse = (mdns_browse_sync_t *)mdns_mem_malloc(sizeof(mdns_browse_sync_t));
                        if (!out_sync_browse) {
                            HOOK_MALLOC_FAILED;
                            goto clear_rx_packet;
                        }
                        out_sync_browse->browse = browse_result;
                        out_sync_browse->sync_result = NULL;
                    }
                    if (!browse_result_service) {
                        browse_result_service = (char *)mdns_mem_malloc(MDNS_NAME_BUF_LEN);
                        if (!browse_result_service) {
                            HOOK_MALLOC_FAILED;
                            goto clear_rx_packet;
                        }
                    }
                    memcpy(browse_result_service, browse_result->service, MDNS_NAME_BUF_LEN);
                    if (!browse_result_proto) {
                        browse_result_proto = (char *)mdns_mem_malloc(MDNS_NAME_BUF_LEN);
                        if (!browse_result_proto) {
                            HOOK_MALLOC_FAILED;
                            goto clear_rx_packet;
                        }
                    }
                    memcpy(browse_result_proto, browse_result->proto, MDNS_NAME_BUF_LEN);
                    if (type == MDNS_TYPE_SRV || type == MDNS_TYPE_TXT) {
                        if (!browse_result_instance) {
                            browse_result_instance = (char *)mdns_mem_malloc(MDNS_NAME_BUF_LEN);
                            if (!browse_result_instance) {
                                HOOK_MALLOC_FAILED;
                                goto clear_rx_packet;
                            }
                        }
                        memcpy(browse_result_instance, name->host, MDNS_NAME_BUF_LEN);
                    }
                }
            }

            if (type == MDNS_TYPE_PTR) {
                if (!_mdns_parse_fqdn(data, data_ptr, name, len)) {
                    continue;//error
                }
                if (search_result) {
                    _mdns_search_result_add_ptr(search_result, name->host, name->service, name->proto,
                                                packet->tcpip_if, packet->ip_protocol, ttl);
                } else if ((discovery || ours) && !name->sub && _mdns_name_is_ours(name)) {
                    if (name->host[0]) {
                        service = _mdns_get_service_item_instance(name->host, name->service, name->proto, NULL);
                    } else {
                        service = _mdns_get_service_item(name->service, name->proto, NULL);
                    }
                    if (discovery && service) {
                        _mdns_remove_parsed_question(parsed_packet, MDNS_TYPE_SDPTR, service);
                    } else if (service && parsed_packet->questions && !parsed_packet->probe) {
                        _mdns_remove_parsed_question(parsed_packet, type, service);
                    } else if (service) {
                        //check if TTL is more than half of the full TTL value (4500)
                        if (ttl > (MDNS_ANSWER_PTR_TTL / 2)) {
                            _mdns_remove_scheduled_answer(packet->tcpip_if, packet->ip_protocol, type, service);
                        }
                    }
                    if (service) {
                        mdns_parsed_record_t *record = mdns_mem_malloc(sizeof(mdns_parsed_record_t));
                        if (!record) {
                            HOOK_MALLOC_FAILED;
                            goto clear_rx_packet;
                        }
                        record->next = parsed_packet->records;
                        parsed_packet->records = record;
                        record->type = MDNS_TYPE_PTR;
                        record->record_type = MDNS_ANSWER;
                        record->ttl = ttl;
                        record->host = NULL;
                        record->service = NULL;
                        record->proto = NULL;
                        if (name->host[0]) {
                            record->host = mdns_mem_malloc(MDNS_NAME_BUF_LEN);
                            if (!record->host) {
                                HOOK_MALLOC_FAILED;
                                goto clear_rx_packet;
                            }
                            memcpy(record->host, name->host, MDNS_NAME_BUF_LEN);
                        }
                        if (name->service[0]) {
                            record->service = mdns_mem_malloc(MDNS_NAME_BUF_LEN);
                            if (!record->service) {
                                HOOK_MALLOC_FAILED;
                                goto clear_rx_packet;
                            }
                            memcpy(record->service, name->service, MDNS_NAME_BUF_LEN);
                        }
                        if (name->proto[0]) {
                            record->proto = mdns_mem_malloc(MDNS_NAME_BUF_LEN);
                            if (!record->proto) {
                                HOOK_MALLOC_FAILED;
                                goto clear_rx_packet;
                            }
                            memcpy(record->proto, name->proto, MDNS_NAME_BUF_LEN);
                        }
                    }
                }
            } else if (type == MDNS_TYPE_SRV) {
                mdns_result_t *result = NULL;
                if (search_result && search_result->type == MDNS_TYPE_PTR) {
                    result = search_result->result;
                    while (result) {
                        if (_mdns_get_esp_netif(packet->tcpip_if) == result->esp_netif
                                && packet->ip_protocol == result->ip_protocol
                                && result->instance_name && !strcmp(name->host, result->instance_name)) {
                            break;
                        }
                        result = result->next;
                    }
                    if (!result) {
                        result = _mdns_search_result_add_ptr(search_result, name->host, name->service, name->proto,
                                                             packet->tcpip_if, packet->ip_protocol, ttl);
                        if (!result) {
                            continue;//error
                        }
                    }
                }
                bool is_selfhosted = _mdns_name_is_selfhosted(name);
                if (!_mdns_parse_fqdn(data, data_ptr + MDNS_SRV_FQDN_OFFSET, name, len)) {
                    continue;//error
                }
                if (data_ptr + MDNS_SRV_PORT_OFFSET + 1 >= data + len) {
                    goto clear_rx_packet; // malformed packet, won't read behind it
                }
                uint16_t priority = mdns_utils_read_u16(data_ptr, MDNS_SRV_PRIORITY_OFFSET);
                uint16_t weight = mdns_utils_read_u16(data_ptr, MDNS_SRV_WEIGHT_OFFSET);
                uint16_t port = mdns_utils_read_u16(data_ptr, MDNS_SRV_PORT_OFFSET);

                if (browse_result) {
                    _mdns_browse_result_add_srv(browse_result, name->host, browse_result_instance, browse_result_service,
                                                browse_result_proto, port, packet->tcpip_if, packet->ip_protocol, ttl, out_sync_browse);
                }
                if (search_result) {
                    if (search_result->type == MDNS_TYPE_PTR) {
                        if (!result->hostname) { // assign host/port for this entry only if not previously set
                            result->port = port;
                            result->hostname = mdns_mem_strdup(name->host);
                        }
                    } else {
                        _mdns_search_result_add_srv(search_result, name->host, port, packet->tcpip_if, packet->ip_protocol, ttl);
                    }
                } else if (ours) {
                    if (parsed_packet->questions && !parsed_packet->probe) {
                        _mdns_remove_parsed_question(parsed_packet, type, service);
                        continue;
                    } else if (parsed_packet->distributed) {
                        _mdns_remove_scheduled_answer(packet->tcpip_if, packet->ip_protocol, type, service);
                        continue;
                    }
                    if (!is_selfhosted) {
                        continue;
                    }
                    //detect collision (-1=won, 0=none, 1=lost)
                    int col = 0;
                    if (mdns_class > 1) {
                        col = 1;
                    } else if (!mdns_class) {
                        col = -1;
                    } else if (service) { // only detect srv collision if service existed
                        col = _mdns_check_srv_collision(service->service, priority, weight, port, name->host, name->domain);
                    }
                    if (service && col && (parsed_packet->probe || parsed_packet->authoritative)) {
                        if (col > 0 || !port) {
                            do_not_reply = true;
                            if (mdns_responder_is_probing(packet)) {
                                mdns_responder_probe_failed(packet);
                                if (!mdns_utils_str_null_or_empty(service->service->instance)) {
                                    char *new_instance = _mdns_mangle_name((char *)service->service->instance);
                                    if (new_instance) {
                                        mdns_mem_free((char *)service->service->instance);
                                        service->service->instance = new_instance;
                                    }
                                    _mdns_probe_all_pcbs(&service, 1, false, false);
                                } else if (!mdns_utils_str_null_or_empty(mdns_utils_get_instance())) {
                                    char *new_instance = _mdns_mangle_name((char *)mdns_utils_get_instance());
                                    if (new_instance) {
                                        mdns_utils_set_instance(new_instance);
                                    }
                                    _mdns_restart_all_pcbs_no_instance();
                                } else {
                                    char *new_host = _mdns_mangle_name((char *)mdns_utils_get_global_hostname());
                                    if (new_host) {
                                        _mdns_remap_self_service_hostname(mdns_utils_get_global_hostname(), new_host);
                                        mdns_utils_set_global_hostname(new_host);
                                    }
                                    _mdns_restart_all_pcbs();
                                }
                            } else if (service) {
                                _mdns_pcb_send_bye(packet->tcpip_if, packet->ip_protocol, &service, 1, false);
                                _mdns_init_pcb_probe(packet->tcpip_if, packet->ip_protocol, &service, 1, false);
                            }
                        }
                    } else if (ttl > 60 && !col && !parsed_packet->authoritative && !parsed_packet->probe && !parsed_packet->questions) {
                        _mdns_remove_scheduled_answer(packet->tcpip_if, packet->ip_protocol, type, service);
                    }
                }
            } else if (type == MDNS_TYPE_TXT) {
                mdns_txt_item_t *txt = NULL;
                uint8_t *txt_value_len = NULL;
                size_t txt_count = 0;

                mdns_result_t *result = NULL;
                if (browse_result) {
                    _mdns_result_txt_create(data_ptr, data_len, &txt, &txt_value_len, &txt_count);
                    _mdns_browse_result_add_txt(browse_result, browse_result_instance, browse_result_service, browse_result_proto,
                                                txt, txt_value_len, txt_count, packet->tcpip_if, packet->ip_protocol, ttl, out_sync_browse);
                }
                if (search_result) {
                    if (search_result->type == MDNS_TYPE_PTR) {
                        result = search_result->result;
                        while (result) {
                            if (_mdns_get_esp_netif(packet->tcpip_if) == result->esp_netif
                                    && packet->ip_protocol == result->ip_protocol
                                    && result->instance_name && !strcmp(name->host, result->instance_name)) {
                                break;
                            }
                            result = result->next;
                        }
                        if (!result) {
                            result = _mdns_search_result_add_ptr(search_result, name->host, name->service, name->proto,
                                                                 packet->tcpip_if, packet->ip_protocol, ttl);
                            if (!result) {
                                continue;//error
                            }
                        }
                        if (!result->txt) {
                            _mdns_result_txt_create(data_ptr, data_len, &txt, &txt_value_len, &txt_count);
                            if (txt_count) {
                                result->txt = txt;
                                result->txt_count = txt_count;
                                result->txt_value_len = txt_value_len;
                            }
                        }
                    } else {
                        _mdns_result_txt_create(data_ptr, data_len, &txt, &txt_value_len, &txt_count);
                        if (txt_count) {
                            _mdns_search_result_add_txt(search_result, txt, txt_value_len, txt_count, packet->tcpip_if, packet->ip_protocol, ttl);
                        }
                    }
                } else if (ours) {
                    if (parsed_packet->questions && !parsed_packet->probe && service) {
                        _mdns_remove_parsed_question(parsed_packet, type, service);
                        continue;
                    }
                    if (!_mdns_name_is_selfhosted(name)) {
                        continue;
                    }
                    //detect collision (-1=won, 0=none, 1=lost)
                    int col = 0;
                    if (mdns_class > 1) {
                        col = 1;
                    } else if (!mdns_class) {
                        col = -1;
                    } else if (service) { // only detect txt collision if service existed
                        col = _mdns_check_txt_collision(service->service, data_ptr, data_len);
                    }
                    if (col && !mdns_responder_is_probing(packet) && service) {
                        do_not_reply = true;
                        _mdns_init_pcb_probe(packet->tcpip_if, packet->ip_protocol, &service, 1, true);
                    } else if (ttl > (MDNS_ANSWER_TXT_TTL / 2) && !col && !parsed_packet->authoritative && !parsed_packet->probe && !parsed_packet->questions && !mdns_responder_is_probing(packet)) {
                        _mdns_remove_scheduled_answer(packet->tcpip_if, packet->ip_protocol, type, service);
                    }
                }

            }
#ifdef CONFIG_LWIP_IPV6
            else if (type == MDNS_TYPE_AAAA) {//ipv6
                esp_ip_addr_t ip6;
                ip6.type = ESP_IPADDR_TYPE_V6;
                memcpy(ip6.u_addr.ip6.addr, data_ptr, MDNS_ANSWER_AAAA_SIZE);
                if (browse_result) {
                    _mdns_browse_result_add_ip(browse_result, name->host, &ip6, packet->tcpip_if, packet->ip_protocol, ttl, out_sync_browse);
                }
                if (search_result) {
                    //check for more applicable searches (PTR & A/AAAA at the same time)
                    while (search_result) {
                        _mdns_search_result_add_ip(search_result, name->host, &ip6, packet->tcpip_if, packet->ip_protocol, ttl);
                        search_result = _mdns_search_find_from(search_result->next, name, type, packet->tcpip_if, packet->ip_protocol);
                    }
                } else if (ours) {
                    if (parsed_packet->questions && !parsed_packet->probe) {
                        _mdns_remove_parsed_question(parsed_packet, type, NULL);
                        continue;
                    }
                    if (!_mdns_name_is_selfhosted(name)) {
                        continue;
                    }
                    //detect collision (-1=won, 0=none, 1=lost)
                    int col = 0;
                    if (mdns_class > 1) {
                        col = 1;
                    } else if (!mdns_class) {
                        col = -1;
                    } else {
                        col = _mdns_check_aaaa_collision(&(ip6.u_addr.ip6), packet->tcpip_if);
                    }
                    if (col == 2) {
                        goto clear_rx_packet;
                    } else if (col == 1) {
                        do_not_reply = true;
                        if (mdns_responder_is_probing(packet)) {
                            if (col && (parsed_packet->probe || parsed_packet->authoritative)) {
                                mdns_responder_probe_failed(packet);
                                char *new_host = _mdns_mangle_name((char *)mdns_utils_get_global_hostname());
                                if (new_host) {
                                    _mdns_remap_self_service_hostname(mdns_utils_get_global_hostname(), new_host);
                                    mdns_utils_set_global_hostname(new_host);
                                }
                                _mdns_restart_all_pcbs();
                            }
                        } else {
                            _mdns_init_pcb_probe(packet->tcpip_if, packet->ip_protocol, NULL, 0, true);
                        }
                    } else if (ttl > 60 && !col && !parsed_packet->authoritative && !parsed_packet->probe && !parsed_packet->questions && !mdns_responder_is_probing(packet)) {
                        _mdns_remove_scheduled_answer(packet->tcpip_if, packet->ip_protocol, type, NULL);
                    }
                }

            }
#endif /* CONFIG_LWIP_IPV6 */
#ifdef CONFIG_LWIP_IPV4
            else if (type == MDNS_TYPE_A) {
                esp_ip_addr_t ip;
                ip.type = ESP_IPADDR_TYPE_V4;
                memcpy(&(ip.u_addr.ip4.addr), data_ptr, 4);
                if (browse_result) {
                    _mdns_browse_result_add_ip(browse_result, name->host, &ip, packet->tcpip_if, packet->ip_protocol, ttl, out_sync_browse);
                }
                if (search_result) {
                    //check for more applicable searches (PTR & A/AAAA at the same time)
                    while (search_result) {
                        _mdns_search_result_add_ip(search_result, name->host, &ip, packet->tcpip_if, packet->ip_protocol, ttl);
                        search_result = _mdns_search_find_from(search_result->next, name, type, packet->tcpip_if, packet->ip_protocol);
                    }
                } else if (ours) {
                    if (parsed_packet->questions && !parsed_packet->probe) {
                        _mdns_remove_parsed_question(parsed_packet, type, NULL);
                        continue;
                    }
                    if (!_mdns_name_is_selfhosted(name)) {
                        continue;
                    }
                    //detect collision (-1=won, 0=none, 1=lost)
                    int col = 0;
                    if (mdns_class > 1) {
                        col = 1;
                    } else if (!mdns_class) {
                        col = -1;
                    } else {
                        col = _mdns_check_a_collision(&(ip.u_addr.ip4), packet->tcpip_if);
                    }
                    if (col == 2) {
                        goto clear_rx_packet;
                    } else if (col == 1) {
                        do_not_reply = true;
                        if (mdns_responder_is_probing(packet)) {
                            if (col && (parsed_packet->probe || parsed_packet->authoritative)) {
                                mdns_responder_probe_failed(packet);
                                char *new_host = _mdns_mangle_name((char *)mdns_utils_get_global_hostname());
                                if (new_host) {
                                    _mdns_remap_self_service_hostname(mdns_utils_get_global_hostname(), new_host);
                                    mdns_utils_set_global_hostname(new_host);
                                }
                                _mdns_restart_all_pcbs();
                            }
                        } else {
                            _mdns_init_pcb_probe(packet->tcpip_if, packet->ip_protocol, NULL, 0, true);
                        }
                    } else if (ttl > 60 && !col && !parsed_packet->authoritative && !parsed_packet->probe && !parsed_packet->questions && !mdns_responder_is_probing(packet)) {
                        _mdns_remove_scheduled_answer(packet->tcpip_if, packet->ip_protocol, type, NULL);
                    }
                }

            }
#endif /* CONFIG_LWIP_IPV4 */
        }
        //end while
        if (parsed_packet->authoritative) {
            _mdns_search_finish_done();
        }
    }

    if (!do_not_reply && mdns_responder_after_probing(packet) && (parsed_packet->questions || parsed_packet->discovery)) {
        _mdns_create_answer_from_parsed_packet(parsed_packet);
    }
    if (out_sync_browse) {
        DBG_BROWSE_RESULTS_WITH_MSG(out_sync_browse->browse->result,
                                    "Browse %s%s total result:", out_sync_browse->browse->service, out_sync_browse->browse->proto);
        if (out_sync_browse->sync_result) {
            DBG_BROWSE_RESULTS_WITH_MSG(out_sync_browse->sync_result->result,
                                        "Changed result:");
            _mdns_sync_browse_action(ACTION_BROWSE_SYNC, out_sync_browse);
        } else {
            mdns_mem_free(out_sync_browse);
        }
        out_sync_browse = NULL;
    }

clear_rx_packet:
    while (parsed_packet->questions) {
        mdns_parsed_question_t *question = parsed_packet->questions;
        parsed_packet->questions = parsed_packet->questions->next;
        if (question->host) {
            mdns_mem_free(question->host);
        }
        if (question->service) {
            mdns_mem_free(question->service);
        }
        if (question->proto) {
            mdns_mem_free(question->proto);
        }
        if (question->domain) {
            mdns_mem_free(question->domain);
        }
        mdns_mem_free(question);
    }
    while (parsed_packet->records) {
        mdns_parsed_record_t *record = parsed_packet->records;
        parsed_packet->records = parsed_packet->records->next;
        if (record->host) {
            mdns_mem_free(record->host);
        }
        if (record->service) {
            mdns_mem_free(record->service);
        }
        if (record->proto) {
            mdns_mem_free(record->proto);
        }
        record->next = NULL;
        mdns_mem_free(record);
    }
    mdns_mem_free(parsed_packet);
    mdns_mem_free(browse_result_instance);
    mdns_mem_free(browse_result_service);
    mdns_mem_free(browse_result_proto);
    mdns_mem_free(out_sync_browse);
}
