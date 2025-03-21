/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_check.h"
#include "mdns.h"
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_mem_caps.h"
#include "mdns_utils.h"
#include "mdns_debug.h"
#include "mdns_browser.h"
#include "mdns_netif.h"
#include "mdns_send.h"
#include "mdns_receive.h"
#include "mdns_querier.h"
#include "mdns_responder.h"

mdns_server_t *_mdns_server = NULL;
static mdns_host_item_t *_mdns_host_list = NULL;
static mdns_host_item_t _mdns_self_host;

static const char *TAG = "mdns";

static volatile TaskHandle_t _mdns_service_task_handle = NULL;
static SemaphoreHandle_t _mdns_service_semaphore = NULL;
static StackType_t *_mdns_stack_buffer;

esp_err_t mdns_post_custom_action_tcpip_if(mdns_if_t mdns_if, mdns_event_actions_t event_action);

const char *mdns_priv_get_global_hostname(void)
{
    return _mdns_server ? _mdns_server->hostname : NULL;
}

mdns_srv_item_t *mdns_priv_get_services(void)
{
    return _mdns_server->services;
}

mdns_host_item_t *mdns_priv_get_hosts(void)
{
    return _mdns_host_list;
}

mdns_host_item_t *priv_get_self_host(void)
{
    return &_mdns_self_host;
}

void mdns_priv_set_global_hostname(const char *hostname)
{
    if (_mdns_server) {
        if (_mdns_server->hostname) {
            mdns_mem_free((void *)_mdns_server->hostname);
        }
        _mdns_server->hostname = hostname;
        _mdns_self_host.hostname = hostname;
    }
}

const char *mdns_priv_get_instance(void)
{
    return _mdns_server ? _mdns_server->instance : NULL;
}

void mdns_priv_set_instance(const char *instance)
{
    if (_mdns_server) {
        if (_mdns_server->instance) {
            mdns_mem_free((void *)_mdns_server->instance);
        }
        _mdns_server->instance = instance;
    }
}

bool mdns_priv_is_server_init(void)
{
    return _mdns_server != NULL;
}

static inline bool _str_null_or_empty(const char *str)
{
    return (str == NULL || *str == 0);
}

static bool _mdns_can_add_more_services(void)
{
#if MDNS_MAX_SERVICES == 0
    return false;
#else
    mdns_srv_item_t *s = _mdns_server->services;
    uint16_t service_num = 0;
    while (s) {
        service_num ++;
        s = s->next;
        if (service_num >= MDNS_MAX_SERVICES) {
            return false;
        }
    }
    return true;
#endif
}

bool mdns_priv_queue_action(mdns_action_t *action)
{
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        return false;
    }
    return true;
}

/**
 * @brief  Send announcement on all active PCBs
 */
static void _mdns_announce_all_pcbs(mdns_srv_item_t **services, size_t len, bool include_ip)
{
    uint8_t i, j;
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            mdns_priv_pcb_announce((mdns_if_t) i, (mdns_ip_protocol_t) j, services, len, include_ip);
        }
    }
}

/**
 * @brief  Restart the responder on all active PCBs
 */
static void _mdns_send_final_bye(bool include_ip)
{
    //collect all services and start probe
    size_t srv_count = 0;
    mdns_srv_item_t *a = _mdns_server->services;
    while (a) {
        srv_count++;
        a = a->next;
    }
    if (!srv_count) {
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t i = 0;
    a = _mdns_server->services;
    while (a) {
        services[i++] = a;
        a = a->next;
    }
    mdns_responder_send_bye_service(services, srv_count, include_ip);
}

/**
 * @brief  Stop the responder on all services without instance
 */
static void _mdns_send_bye_all_pcbs_no_instance(bool include_ip)
{
    size_t srv_count = 0;
    mdns_srv_item_t *a = _mdns_server->services;
    while (a) {
        if (!a->service->instance) {
            srv_count++;
        }
        a = a->next;
    }
    if (!srv_count) {
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t i = 0;
    a = _mdns_server->services;
    while (a) {
        if (!a->service->instance) {
            services[i++] = a;
        }
        a = a->next;
    }
    mdns_responder_send_bye_service(services, srv_count, include_ip);
}

void mdns_priv_restart_all_pcbs_no_instance(void)
{
    size_t srv_count = 0;
    mdns_srv_item_t *a = _mdns_server->services;
    while (a) {
        if (!a->service->instance) {
            srv_count++;
        }
        a = a->next;
    }
    if (!srv_count) {
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t i = 0;
    a = _mdns_server->services;
    while (a) {
        if (!a->service->instance) {
            services[i++] = a;
        }
        a = a->next;
    }
    mdns_responder_probe_all_pcbs(services, srv_count, false, true);
}

void mdns_priv_restart_all_pcbs(void)
{
    _mdns_clear_tx_queue_head();
    size_t srv_count = 0;
    mdns_srv_item_t *a = _mdns_server->services;
    while (a) {
        srv_count++;
        a = a->next;
    }
    if (srv_count == 0) {
        mdns_responder_probe_all_pcbs(NULL, 0, true, true);
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t l = 0;
    a = _mdns_server->services;
    while (a) {
        services[l++] = a;
        a = a->next;
    }

    mdns_responder_probe_all_pcbs(services, srv_count, true, true);
}



/**
 * @brief  creates/allocates new text item list
 * @param  num_items     service number of txt items or 0
 * @param  txt           service txt items array or NULL
 *
 * @return pointer to the linked txt item list or NULL
 */
static mdns_txt_linked_item_t *_mdns_allocate_txt(size_t num_items, mdns_txt_item_t txt[])
{
    mdns_txt_linked_item_t *new_txt = NULL;
    size_t i = 0;
    if (num_items) {
        for (i = 0; i < num_items; i++) {
            mdns_txt_linked_item_t *new_item = (mdns_txt_linked_item_t *)mdns_mem_malloc(sizeof(mdns_txt_linked_item_t));
            if (!new_item) {
                HOOK_MALLOC_FAILED;
                break;
            }
            new_item->key = mdns_mem_strdup(txt[i].key);
            if (!new_item->key) {
                mdns_mem_free(new_item);
                break;
            }
            new_item->value = mdns_mem_strdup(txt[i].value);
            if (!new_item->value) {
                mdns_mem_free((char *)new_item->key);
                mdns_mem_free(new_item);
                break;
            }
            new_item->value_len = strlen(new_item->value);
            new_item->next = new_txt;
            new_txt = new_item;
        }
    }
    return new_txt;
}

/**
 * @brief Deallocate the txt linked list
 * @param txt pointer to the txt pointer to free, noop if txt==NULL
 */
static void _mdns_free_linked_txt(mdns_txt_linked_item_t *txt)
{
    mdns_txt_linked_item_t *t;
    while (txt) {
        t = txt;
        txt = txt->next;
        mdns_mem_free((char *)t->value);
        mdns_mem_free((char *)t->key);
        mdns_mem_free(t);
    }
}

/**
 * @brief  creates/allocates new service
 * @param  service       service type
 * @param  proto         service proto
 * @param  hostname      service hostname
 * @param  port          service port
 * @param  instance      service instance
 * @param  num_items     service number of txt items or 0
 * @param  txt           service txt items array or NULL
 *
 * @return pointer to the service or NULL on error
 */
static mdns_service_t *_mdns_create_service(const char *service, const char *proto, const char *hostname,
                                            uint16_t port, const char *instance, size_t num_items,
                                            mdns_txt_item_t txt[])
{
    mdns_service_t *s = (mdns_service_t *)mdns_mem_calloc(1, sizeof(mdns_service_t));
    if (!s) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }

    mdns_txt_linked_item_t *new_txt = _mdns_allocate_txt(num_items, txt);
    if (num_items && new_txt == NULL) {
        goto fail;
    }

    s->priority = 0;
    s->weight = 0;
    s->instance = instance ? mdns_mem_strndup(instance, MDNS_NAME_BUF_LEN - 1) : NULL;
    s->txt = new_txt;
    s->port = port;
    s->subtype = NULL;

    if (hostname) {
        s->hostname = mdns_mem_strndup(hostname, MDNS_NAME_BUF_LEN - 1);
        if (!s->hostname) {
            goto fail;
        }
    } else {
        s->hostname = NULL;
    }

    s->service = mdns_mem_strndup(service, MDNS_NAME_BUF_LEN - 1);
    if (!s->service) {
        goto fail;
    }

    s->proto = mdns_mem_strndup(proto, MDNS_NAME_BUF_LEN - 1);
    if (!s->proto) {
        goto fail;
    }
    return s;

fail:
    _mdns_free_linked_txt(s->txt);
    mdns_mem_free((char *)s->instance);
    mdns_mem_free((char *)s->service);
    mdns_mem_free((char *)s->proto);
    mdns_mem_free((char *)s->hostname);
    mdns_mem_free(s);

    return NULL;
}

static void _mdns_free_subtype(mdns_subtype_t *subtype)
{
    while (subtype) {
        mdns_subtype_t *next = subtype->next;
        mdns_mem_free((char *)subtype->subtype);
        mdns_mem_free(subtype);
        subtype = next;
    }
}

static void _mdns_free_service_subtype(mdns_service_t *service)
{
    _mdns_free_subtype(service->subtype);
    service->subtype = NULL;
}

/**
 * @brief  free service memory
 *
 * @param  service      the service
 */
static void _mdns_free_service(mdns_service_t *service)
{
    if (!service) {
        return;
    }
    mdns_mem_free((char *)service->instance);
    mdns_mem_free((char *)service->service);
    mdns_mem_free((char *)service->proto);
    mdns_mem_free((char *)service->hostname);
    while (service->txt) {
        mdns_txt_linked_item_t *s = service->txt;
        service->txt = service->txt->next;
        mdns_mem_free((char *)s->key);
        mdns_mem_free((char *)s->value);
        mdns_mem_free(s);
    }
    _mdns_free_service_subtype(service);
    mdns_mem_free(service);
}

/**
 * @brief Adds a delegated hostname to the linked list
 * @param hostname Host name pointer
 * @param address_list Address list
 * @return  true on success
 *          false if the host wasn't attached (this is our hostname, or alloc failure) so we have to free the structs
 */
static bool _mdns_delegate_hostname_add(const char *hostname, mdns_ip_addr_t *address_list)
{
    if (mdns_utils_hostname_is_ours(hostname)) {
        return false;
    }

    mdns_host_item_t *host = (mdns_host_item_t *)mdns_mem_malloc(sizeof(mdns_host_item_t));

    if (host == NULL) {
        return false;
    }
    host->address_list = address_list;
    host->hostname = hostname;
    host->next = _mdns_host_list;
    _mdns_host_list = host;
    return true;
}



static bool _mdns_delegate_hostname_set_address(const char *hostname, mdns_ip_addr_t *address_list)
{
    if (!_str_null_or_empty(_mdns_server->hostname) &&
            strcasecmp(hostname, _mdns_server->hostname) == 0) {
        return false;
    }
    mdns_host_item_t *host = _mdns_host_list;
    while (host != NULL) {
        if (strcasecmp(hostname, host->hostname) == 0) {
            // free previous address list
            mdns_utils_free_address_list(host->address_list);
            // set current address list to the host
            host->address_list = address_list;
            return true;
        }
        host = host->next;
    }
    return false;
}



static void free_delegated_hostnames(void)
{
    mdns_host_item_t *host = _mdns_host_list;
    while (host != NULL) {
        mdns_utils_free_address_list(host->address_list);
        mdns_mem_free((char *)host->hostname);
        mdns_host_item_t *item = host;
        host = host->next;
        mdns_mem_free(item);
    }
    _mdns_host_list = NULL;
}

static bool _mdns_delegate_hostname_remove(const char *hostname)
{
    mdns_srv_item_t *srv = _mdns_server->services;
    mdns_srv_item_t *prev_srv = NULL;
    while (srv) {
        if (strcasecmp(srv->service->hostname, hostname) == 0) {
            mdns_srv_item_t *to_free = srv;
            mdns_responder_send_bye_service(&srv, 1, false);
            _mdns_remove_scheduled_service_packets(srv->service);
            if (prev_srv == NULL) {
                _mdns_server->services = srv->next;
                srv = srv->next;
            } else {
                prev_srv->next = srv->next;
                srv = srv->next;
            }
            _mdns_free_service(to_free->service);
            mdns_mem_free(to_free);
        } else {
            prev_srv = srv;
            srv = srv->next;
        }
    }
    mdns_host_item_t *host = _mdns_host_list;
    mdns_host_item_t *prev_host = NULL;
    while (host != NULL) {
        if (strcasecmp(hostname, host->hostname) == 0) {
            if (prev_host == NULL) {
                _mdns_host_list = host->next;
            } else {
                prev_host->next = host->next;
            }
            mdns_utils_free_address_list(host->address_list);
            mdns_mem_free((char *)host->hostname);
            mdns_mem_free(host);
            break;
        } else {
            prev_host = host;
            host = host->next;
        }
    }
    return true;
}

#ifdef CONFIG_MDNS_RESPOND_REVERSE_QUERIES
static inline char nibble_to_hex(int var)
{
    return var > 9 ?  var - 10 + 'a' : var + '0';
}
#endif

/**
 * @brief  Performs interface changes based on system events or custom commands
 */
static void perform_event_action(mdns_if_t mdns_if, mdns_event_actions_t action)
{
    if (!_mdns_server || mdns_if >= MDNS_MAX_INTERFACES) {
        return;
    }
    if (action & MDNS_EVENT_ENABLE_IP4) {
        mdns_priv_pcb_enable(mdns_if, MDNS_IP_PROTOCOL_V4);
    }
    if (action & MDNS_EVENT_ENABLE_IP6) {
        mdns_priv_pcb_enable(mdns_if, MDNS_IP_PROTOCOL_V6);
    }
    if (action & MDNS_EVENT_DISABLE_IP4) {
        mdns_priv_pcb_disable(mdns_if, MDNS_IP_PROTOCOL_V4);
    }
    if (action & MDNS_EVENT_DISABLE_IP6) {
        mdns_priv_pcb_disable(mdns_if, MDNS_IP_PROTOCOL_V6);
    }
    if (action & MDNS_EVENT_ANNOUNCE_IP4) {
        mdns_priv_pcb_announce(mdns_if, MDNS_IP_PROTOCOL_V4, NULL, 0, true);
    }
    if (action & MDNS_EVENT_ANNOUNCE_IP6) {
        mdns_priv_pcb_announce(mdns_if, MDNS_IP_PROTOCOL_V6, NULL, 0, true);
    }

#ifdef CONFIG_MDNS_RESPOND_REVERSE_QUERIES
#ifdef CONFIG_LWIP_IPV4
    if (action & MDNS_EVENT_IP4_REVERSE_LOOKUP) {
        esp_netif_ip_info_t if_ip_info;
        if (esp_netif_get_ip_info(mdns_netif_get_esp_netif(mdns_if), &if_ip_info) == ESP_OK) {
            esp_ip4_addr_t *ip = &if_ip_info.ip;
            char *reverse_query_name = NULL;
            if (asprintf(&reverse_query_name, "%d.%d.%d.%d.in-addr",
                         esp_ip4_addr4_16(ip), esp_ip4_addr3_16(ip),
                         esp_ip4_addr2_16(ip), esp_ip4_addr1_16(ip)) > 0 && reverse_query_name) {
                ESP_LOGD(TAG, "Registered reverse query: %s.arpa", reverse_query_name);
                _mdns_delegate_hostname_add(reverse_query_name, NULL);
            }
        }
    }
#endif /* CONFIG_LWIP_IPV4 */
#ifdef CONFIG_LWIP_IPV6
    if (action & MDNS_EVENT_IP6_REVERSE_LOOKUP) {
        esp_ip6_addr_t addr6;
        if (!esp_netif_get_ip6_linklocal(mdns_netif_get_esp_netif(mdns_if), &addr6) && !mdns_utils_ipv6_address_is_zero(addr6)) {
            uint8_t *paddr = (uint8_t *)&addr6.addr;
            const char sub[] = "ip6";
            const size_t query_name_size = 4 * sizeof(addr6.addr) /* (2 nibbles + 2 dots)/per byte of IP address */ + sizeof(sub);
            char *reverse_query_name = mdns_mem_malloc(query_name_size);
            if (reverse_query_name) {
                char *ptr = &reverse_query_name[query_name_size];   // point to the end
                memcpy(ptr - sizeof(sub), sub, sizeof(sub));        // copy the IP sub-domain
                ptr -= sizeof(sub) + 1;                             // move before the sub-domain
                while (reverse_query_name < ptr) {                  // continue populating reverse query from the end
                    *ptr-- = '.';                                   // nibble by nibble, until we reach the beginning
                    *ptr-- = nibble_to_hex(((*paddr) >> 4) & 0x0F);
                    *ptr-- = '.';
                    *ptr-- = nibble_to_hex((*paddr) & 0x0F);
                    paddr++;
                }
                ESP_LOGD(TAG, "Registered reverse query: %s.arpa", reverse_query_name);
                _mdns_delegate_hostname_add(reverse_query_name, NULL);
            }
        }
    }
#endif /* CONFIG_LWIP_IPV6 */
#endif /* CONFIG_MDNS_RESPOND_REVERSE_QUERIES */
}

void mdns_priv_remap_self_service_hostname(const char *old_hostname, const char *new_hostname)
{
    mdns_srv_item_t *service = _mdns_server->services;

    while (service) {
        if (service->service->hostname &&
                strcmp(service->service->hostname, old_hostname) == 0) {
            mdns_mem_free((char *)service->service->hostname);
            service->service->hostname = mdns_mem_strdup(new_hostname);
        }
        service = service->next;
    }
}

/**
 * @brief  Free action data
 */
static void _mdns_free_action(mdns_action_t *action)
{
    switch (action->type) {
    case ACTION_HOSTNAME_SET:
        mdns_mem_free(action->data.hostname_set.hostname);
        break;
    case ACTION_INSTANCE_SET:
        mdns_mem_free(action->data.instance);
        break;
    case ACTION_SEARCH_ADD:
    case ACTION_SEARCH_SEND:
    case ACTION_SEARCH_END:
        mdns_priv_query_action(action, ACTION_CLEANUP);
        break;
    case ACTION_BROWSE_ADD:
    case ACTION_BROWSE_END:
    case ACTION_BROWSE_SYNC:
        mdns_browse_action(action, ACTION_CLEANUP);
        break;
    case ACTION_TX_HANDLE:
        mdns_send_action(action, ACTION_CLEANUP);
        break;
    case ACTION_RX_HANDLE:
        mdns_priv_receive_action(action, ACTION_CLEANUP);
        break;
    case ACTION_DELEGATE_HOSTNAME_SET_ADDR:
    case ACTION_DELEGATE_HOSTNAME_ADD:
        mdns_mem_free((char *)action->data.delegate_hostname.hostname);
        mdns_utils_free_address_list(action->data.delegate_hostname.address_list);
        break;
    case ACTION_DELEGATE_HOSTNAME_REMOVE:
        mdns_mem_free((char *)action->data.delegate_hostname.hostname);
        break;
    default:
        break;
    }
    mdns_mem_free(action);
}

/**
 * @brief  Called from service thread to execute given action
 */
static void _mdns_execute_action(mdns_action_t *action)
{
    switch (action->type) {
    case ACTION_SYSTEM_EVENT:
        perform_event_action(action->data.sys_event.interface, action->data.sys_event.event_action);
        break;
    case ACTION_HOSTNAME_SET:
        _mdns_send_bye_all_pcbs_no_instance(true);
        mdns_priv_remap_self_service_hostname(_mdns_server->hostname, action->data.hostname_set.hostname);
        mdns_mem_free((char *)_mdns_server->hostname);
        _mdns_server->hostname = action->data.hostname_set.hostname;
        _mdns_self_host.hostname = action->data.hostname_set.hostname;
        mdns_priv_restart_all_pcbs();
        xSemaphoreGive(_mdns_server->action_sema);
        break;
    case ACTION_INSTANCE_SET:
        _mdns_send_bye_all_pcbs_no_instance(false);
        mdns_mem_free((char *)_mdns_server->instance);
        _mdns_server->instance = action->data.instance;
        mdns_priv_restart_all_pcbs_no_instance();

        break;
    case ACTION_SEARCH_ADD:
    case ACTION_SEARCH_SEND:
    case ACTION_SEARCH_END:
        mdns_priv_query_action(action, ACTION_RUN);
        break;
    case ACTION_BROWSE_ADD:
    case ACTION_BROWSE_SYNC:
    case ACTION_BROWSE_END:
        mdns_browse_action(action, ACTION_RUN);
        break;

    case ACTION_TX_HANDLE:
        mdns_send_action(action, ACTION_RUN);
        break;
    case ACTION_RX_HANDLE:
        mdns_priv_receive_action(action, ACTION_RUN);
        break;
    case ACTION_DELEGATE_HOSTNAME_ADD:
        if (!_mdns_delegate_hostname_add(action->data.delegate_hostname.hostname,
                                         action->data.delegate_hostname.address_list)) {
            mdns_mem_free((char *)action->data.delegate_hostname.hostname);
            mdns_utils_free_address_list(action->data.delegate_hostname.address_list);
        }
        xSemaphoreGive(_mdns_server->action_sema);
        break;
    case ACTION_DELEGATE_HOSTNAME_SET_ADDR:
        if (!_mdns_delegate_hostname_set_address(action->data.delegate_hostname.hostname,
                                                 action->data.delegate_hostname.address_list)) {
            mdns_utils_free_address_list(action->data.delegate_hostname.address_list);
        }
        mdns_mem_free((char *)action->data.delegate_hostname.hostname);
        break;
    case ACTION_DELEGATE_HOSTNAME_REMOVE:
        _mdns_delegate_hostname_remove(action->data.delegate_hostname.hostname);
        mdns_mem_free((char *)action->data.delegate_hostname.hostname);
        break;
    default:
        break;
    }
    mdns_mem_free(action);
}

/**
 * @brief  the main MDNS service task. Packets are received and parsed here
 */
static void _mdns_service_task(void *pvParameters)
{
    mdns_action_t *a = NULL;
    for (;;) {
        if (_mdns_server && _mdns_server->action_queue) {
            if (xQueueReceive(_mdns_server->action_queue, &a, portMAX_DELAY) == pdTRUE) {
                assert(a);
                if (a->type == ACTION_TASK_STOP) {
                    break;
                }
                MDNS_SERVICE_LOCK();
                _mdns_execute_action(a);
                MDNS_SERVICE_UNLOCK();
            }
        } else {
            vTaskDelay(500 * portTICK_PERIOD_MS);
        }
    }
    _mdns_service_task_handle = NULL;
    vTaskDelete(NULL);
}

static void _mdns_timer_cb(void *arg)
{
    mdns_send_packets();
    mdns_priv_query_start_stop();
}

static esp_err_t _mdns_start_timer(void)
{
    esp_timer_create_args_t timer_conf = {
        .callback = _mdns_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mdns_timer"
    };
    esp_err_t err = esp_timer_create(&timer_conf, &(_mdns_server->timer_handle));
    if (err) {
        return err;
    }
    return esp_timer_start_periodic(_mdns_server->timer_handle, MDNS_TIMER_PERIOD_US);
}

static esp_err_t _mdns_stop_timer(void)
{
    esp_err_t err = ESP_OK;
    if (_mdns_server->timer_handle) {
        err = esp_timer_stop(_mdns_server->timer_handle);
        if (err) {
            return err;
        }
        err = esp_timer_delete(_mdns_server->timer_handle);
    }
    return err;
}

static esp_err_t _mdns_task_create_with_caps(void)
{
    esp_err_t ret = ESP_OK;
    static StaticTask_t mdns_task_buffer;

    _mdns_stack_buffer = mdns_mem_task_malloc(MDNS_SERVICE_STACK_DEPTH);
    ESP_GOTO_ON_FALSE(_mdns_stack_buffer != NULL, ESP_FAIL, err, TAG, "failed to allocate memory for the mDNS task's stack");

    _mdns_service_task_handle = xTaskCreateStaticPinnedToCore(_mdns_service_task, "mdns", MDNS_SERVICE_STACK_DEPTH, NULL, MDNS_TASK_PRIORITY, _mdns_stack_buffer, &mdns_task_buffer, MDNS_TASK_AFFINITY);
    ESP_GOTO_ON_FALSE(_mdns_service_task_handle != NULL, ESP_FAIL, err, TAG, "failed to create task for the mDNS");

    return ret;

err:
    mdns_mem_task_free(_mdns_stack_buffer);
    return ret;
}

/**
 * @brief  Start the service thread if not running
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t _mdns_service_task_start(void)
{
    esp_err_t ret = ESP_OK;
    if (!_mdns_service_semaphore) {
        _mdns_service_semaphore = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(_mdns_service_semaphore != NULL, ESP_FAIL, TAG, "Failed to create the mDNS service lock");
    }
    MDNS_SERVICE_LOCK();
    ESP_GOTO_ON_ERROR(_mdns_start_timer(), err, TAG, "Failed to start the mDNS service timer");

    if (!_mdns_service_task_handle) {
        ESP_GOTO_ON_ERROR(_mdns_task_create_with_caps(), err_stop_timer, TAG, "Failed to start the mDNS service task");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0) && !CONFIG_IDF_TARGET_LINUX
        StackType_t *mdns_debug_stack_buffer;
        StaticTask_t *mdns_debug_task_buffer;
        xTaskGetStaticBuffers(_mdns_service_task_handle, &mdns_debug_stack_buffer, &mdns_debug_task_buffer);
        ESP_LOGD(TAG, "mdns_debug_stack_buffer:%p mdns_debug_task_buffer:%p\n", mdns_debug_stack_buffer, mdns_debug_task_buffer);
#endif // CONFIG_IDF_TARGET_LINUX
    }
    MDNS_SERVICE_UNLOCK();
    return ret;

err_stop_timer:
    _mdns_stop_timer();
err:
    MDNS_SERVICE_UNLOCK();
    vSemaphoreDelete(_mdns_service_semaphore);
    _mdns_service_semaphore = NULL;
    return ret;
}

/**
 * @brief  Stop the service thread
 *
 * @return
 *      - ESP_OK
 */
static esp_err_t _mdns_service_task_stop(void)
{
    _mdns_stop_timer();
    if (_mdns_service_task_handle) {
        mdns_action_t action;
        mdns_action_t *a = &action;
        action.type = ACTION_TASK_STOP;
        if (xQueueSend(_mdns_server->action_queue, &a, (TickType_t)0) != pdPASS) {
            vTaskDelete(_mdns_service_task_handle);
            _mdns_service_task_handle = NULL;
        }
        while (_mdns_service_task_handle) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    vSemaphoreDelete(_mdns_service_semaphore);
    _mdns_service_semaphore = NULL;
    return ESP_OK;
}

void mdns_priv_service_lock(void)
{
    MDNS_SERVICE_LOCK();
}

void mdns_priv_service_unlock(void)
{
    MDNS_SERVICE_UNLOCK();
}

esp_err_t mdns_init(void)
{
    esp_err_t err = ESP_OK;

    if (_mdns_server) {
        return err;
    }

    _mdns_server = (mdns_server_t *)mdns_mem_malloc(sizeof(mdns_server_t));
    if (!_mdns_server) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }
    memset((uint8_t *)_mdns_server, 0, sizeof(mdns_server_t));

    _mdns_server->action_queue = xQueueCreate(MDNS_ACTION_QUEUE_LEN, sizeof(mdns_action_t *));
    if (!_mdns_server->action_queue) {
        err = ESP_ERR_NO_MEM;
        goto free_server;
    }

    _mdns_server->action_sema = xSemaphoreCreateBinary();
    if (!_mdns_server->action_sema) {
        err = ESP_ERR_NO_MEM;
        goto free_queue;
    }


    if (mdns_netif_init() != ESP_OK) {
        err = ESP_FAIL;
        goto free_action_sema;
    }

    if (_mdns_service_task_start()) {
        //service start failed!
        err = ESP_FAIL;
        goto free_all_and_disable_pcbs;
    }

    return ESP_OK;

free_all_and_disable_pcbs:
    mdns_netif_deinit();
free_action_sema:
    vSemaphoreDelete(_mdns_server->action_sema);
free_queue:
    vQueueDelete(_mdns_server->action_queue);
free_server:
    mdns_mem_free(_mdns_server);
    _mdns_server = NULL;
    return err;
}

void mdns_free(void)
{
    if (!_mdns_server) {
        return;
    }

    // Unregister handlers before destroying the mdns internals to avoid receiving async events while deinit
    mdns_netif_unregister_predefined_handlers();

    mdns_service_remove_all();
    free_delegated_hostnames();
    _mdns_service_task_stop();
    // at this point, the service task is deleted, so we can destroy the stack size
    mdns_mem_task_free(_mdns_stack_buffer);
    mdns_priv_pcb_deinit();
    mdns_mem_free((char *)_mdns_server->hostname);
    mdns_mem_free((char *)_mdns_server->instance);
    if (_mdns_server->action_queue) {
        mdns_action_t *c;
        while (xQueueReceive(_mdns_server->action_queue, &c, 0) == pdTRUE) {
            _mdns_free_action(c);
        }
        vQueueDelete(_mdns_server->action_queue);
    }
    _mdns_clear_tx_queue_head();
    mdns_priv_query_free();
    mdns_browse_free();
    vSemaphoreDelete(_mdns_server->action_sema);
    mdns_mem_free(_mdns_server);
    _mdns_server = NULL;
}

esp_err_t mdns_hostname_set(const char *hostname)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_ARG;
    }
    if (_str_null_or_empty(hostname) || strlen(hostname) > (MDNS_NAME_BUF_LEN - 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    char *new_hostname = mdns_mem_strndup(hostname, MDNS_NAME_BUF_LEN - 1);
    if (!new_hostname) {
        return ESP_ERR_NO_MEM;
    }

    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(new_hostname);
        return ESP_ERR_NO_MEM;
    }
    action->type = ACTION_HOSTNAME_SET;
    action->data.hostname_set.hostname = new_hostname;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(new_hostname);
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(_mdns_server->action_sema, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t mdns_hostname_get(char *hostname)
{
    if (!hostname) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!_mdns_server || !_mdns_server->hostname) {
        return ESP_ERR_INVALID_STATE;
    }

    MDNS_SERVICE_LOCK();
    size_t len = strnlen(_mdns_server->hostname, MDNS_NAME_BUF_LEN - 1);
    strncpy(hostname, _mdns_server->hostname, len);
    hostname[len] = 0;
    MDNS_SERVICE_UNLOCK();
    return ESP_OK;
}

esp_err_t mdns_delegate_hostname_add(const char *hostname, const mdns_ip_addr_t *address_list)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (_str_null_or_empty(hostname) || strlen(hostname) > (MDNS_NAME_BUF_LEN - 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    char *new_hostname = mdns_mem_strndup(hostname, MDNS_NAME_BUF_LEN - 1);
    if (!new_hostname) {
        return ESP_ERR_NO_MEM;
    }

    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(new_hostname);
        return ESP_ERR_NO_MEM;
    }
    action->type = ACTION_DELEGATE_HOSTNAME_ADD;
    action->data.delegate_hostname.hostname = new_hostname;
    action->data.delegate_hostname.address_list = mdns_utils_copy_address_list(address_list);
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(new_hostname);
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(_mdns_server->action_sema, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t mdns_delegate_hostname_remove(const char *hostname)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (_str_null_or_empty(hostname) || strlen(hostname) > (MDNS_NAME_BUF_LEN - 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    char *new_hostname = mdns_mem_strndup(hostname, MDNS_NAME_BUF_LEN - 1);
    if (!new_hostname) {
        return ESP_ERR_NO_MEM;
    }

    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(new_hostname);
        return ESP_ERR_NO_MEM;
    }
    action->type = ACTION_DELEGATE_HOSTNAME_REMOVE;
    action->data.delegate_hostname.hostname = new_hostname;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(new_hostname);
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t mdns_delegate_hostname_set_address(const char *hostname, const mdns_ip_addr_t *address_list)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (_str_null_or_empty(hostname) || strlen(hostname) > (MDNS_NAME_BUF_LEN - 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    char *new_hostname = mdns_mem_strndup(hostname, MDNS_NAME_BUF_LEN - 1);
    if (!new_hostname) {
        return ESP_ERR_NO_MEM;
    }

    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(new_hostname);
        return ESP_ERR_NO_MEM;
    }
    action->type = ACTION_DELEGATE_HOSTNAME_SET_ADDR;
    action->data.delegate_hostname.hostname = new_hostname;
    action->data.delegate_hostname.address_list = mdns_utils_copy_address_list(address_list);
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(new_hostname);
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool mdns_hostname_exists(const char *hostname)
{
    bool ret = false;
    MDNS_SERVICE_LOCK();
    ret = mdns_utils_hostname_is_ours(hostname);
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_instance_name_set(const char *instance)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (_str_null_or_empty(instance) || _mdns_server->hostname == NULL || strlen(instance) > (MDNS_NAME_BUF_LEN - 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    char *new_instance = mdns_mem_strndup(instance, MDNS_NAME_BUF_LEN - 1);
    if (!new_instance) {
        return ESP_ERR_NO_MEM;
    }

    mdns_action_t *action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        mdns_mem_free(new_instance);
        return ESP_ERR_NO_MEM;
    }
    action->type = ACTION_INSTANCE_SET;
    action->data.instance = new_instance;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(new_instance);
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/*
 * MDNS SERVICES
 * */

esp_err_t mdns_service_add_for_host(const char *instance, const char *service, const char *proto, const char *host,
                                    uint16_t port, mdns_txt_item_t txt[], size_t num_items)
{
    if (!_mdns_server || _str_null_or_empty(service) || _str_null_or_empty(proto) || !_mdns_server->hostname) {
        return ESP_ERR_INVALID_ARG;
    }

    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;
    mdns_service_t *s = NULL;

    ESP_GOTO_ON_FALSE(_mdns_can_add_more_services(), ESP_ERR_NO_MEM, err, TAG,
                      "Cannot add more services, please increase CONFIG_MDNS_MAX_SERVICES (%d)", CONFIG_MDNS_MAX_SERVICES);

    mdns_srv_item_t *item = mdns_utils_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(!item, ESP_ERR_INVALID_ARG, err, TAG, "Service already exists");

    s = _mdns_create_service(service, proto, hostname, port, instance, num_items, txt);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NO_MEM, err, TAG, "Cannot create service: Out of memory");

    item = (mdns_srv_item_t *)mdns_mem_malloc(sizeof(mdns_srv_item_t));
    ESP_GOTO_ON_FALSE(item, ESP_ERR_NO_MEM, err, TAG, "Cannot create service: Out of memory");

    item->service = s;
    item->next = NULL;

    item->next = _mdns_server->services;
    _mdns_server->services = item;
    mdns_responder_probe_all_pcbs(&item, 1, false, false);
    MDNS_SERVICE_UNLOCK();
    return ESP_OK;

err:
    MDNS_SERVICE_UNLOCK();
    _mdns_free_service(s);
    if (ret == ESP_ERR_NO_MEM) {
        HOOK_MALLOC_FAILED;
    }
    return ret;
}

esp_err_t mdns_service_add(const char *instance, const char *service, const char *proto, uint16_t port,
                           mdns_txt_item_t txt[], size_t num_items)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_add_for_host(instance, service, proto, NULL, port, txt, num_items);
}

bool mdns_service_exists(const char *service_type, const char *proto, const char *hostname)
{
    bool ret = false;
    MDNS_SERVICE_LOCK();
    ret = mdns_utils_get_service_item(service_type, proto, hostname) != NULL;
    MDNS_SERVICE_UNLOCK();
    return ret;
}

bool mdns_service_exists_with_instance(const char *instance, const char *service_type, const char *proto,
                                       const char *hostname)
{
    bool ret = false;
    MDNS_SERVICE_LOCK();
    ret = mdns_utils_get_service_item_instance(instance, service_type, proto, hostname) != NULL;
    MDNS_SERVICE_UNLOCK();
    return ret;
}

static mdns_txt_item_t *_copy_mdns_txt_items(mdns_txt_linked_item_t *items, uint8_t **txt_value_len, size_t *txt_count)
{
    mdns_txt_item_t *ret = NULL;
    size_t ret_index = 0;
    for (mdns_txt_linked_item_t *tmp = items; tmp != NULL; tmp = tmp->next) {
        ret_index++;
    }
    *txt_count = ret_index;
    if (ret_index == 0) {   // handle empty TXT
        *txt_value_len = NULL;
        return NULL;
    }
    ret = (mdns_txt_item_t *)mdns_mem_calloc(ret_index, sizeof(mdns_txt_item_t));
    *txt_value_len = (uint8_t *)mdns_mem_calloc(ret_index, sizeof(uint8_t));
    if (!ret || !(*txt_value_len)) {
        HOOK_MALLOC_FAILED;
        goto handle_error;
    }
    ret_index = 0;
    for (mdns_txt_linked_item_t *tmp = items; tmp != NULL; tmp = tmp->next) {
        size_t key_len = strlen(tmp->key);
        char *key = (char *)mdns_mem_malloc(key_len + 1);
        if (!key) {
            HOOK_MALLOC_FAILED;
            goto handle_error;
        }
        memcpy(key, tmp->key, key_len);
        key[key_len] = 0;
        ret[ret_index].key = key;
        char *value = (char *)mdns_mem_malloc(tmp->value_len + 1);
        if (!value) {
            HOOK_MALLOC_FAILED;
            goto handle_error;
        }
        memcpy(value, tmp->value, tmp->value_len);
        value[tmp->value_len] = 0;
        ret[ret_index].value = value;
        (*txt_value_len)[ret_index] = tmp->value_len;
        ret_index++;
    }
    return ret;

handle_error:
    for (size_t y = 0; y < ret_index + 1 && ret != NULL; y++) {
        mdns_txt_item_t *t = &ret[y];
        mdns_mem_free((char *)t->key);
        mdns_mem_free((char *)t->value);
    }
    mdns_mem_free(*txt_value_len);
    mdns_mem_free(ret);
    return NULL;
}

static mdns_ip_addr_t *_copy_delegated_host_address_list(char *hostname)
{
    mdns_host_item_t *host = _mdns_host_list;
    while (host) {
        if (strcasecmp(host->hostname, hostname) == 0) {
            return mdns_utils_copy_address_list(host->address_list);
        }
        host = host->next;
    }
    return NULL;
}

static mdns_result_t *_mdns_lookup_service(const char *instance, const char *service, const char *proto, size_t max_results, bool selfhost)
{
    if (_str_null_or_empty(service) || _str_null_or_empty(proto)) {
        return NULL;
    }
    mdns_result_t *results = NULL;
    size_t num_results = 0;
    mdns_srv_item_t *s = _mdns_server->services;
    while (s) {
        mdns_service_t *srv = s->service;
        if (!srv || !srv->hostname) {
            s = s->next;
            continue;
        }
        bool is_service_selfhosted = !_str_null_or_empty(_mdns_server->hostname) && !strcasecmp(_mdns_server->hostname, srv->hostname);
        bool is_service_delegated = _str_null_or_empty(_mdns_server->hostname) || strcasecmp(_mdns_server->hostname, srv->hostname);
        if ((selfhost && is_service_selfhosted) || (!selfhost && is_service_delegated)) {
            if (!strcasecmp(srv->service, service) && !strcasecmp(srv->proto, proto) &&
                    (_str_null_or_empty(instance) || mdns_utils_instance_name_match(srv->instance, instance))) {
                mdns_result_t *item = (mdns_result_t *)mdns_mem_malloc(sizeof(mdns_result_t));
                if (!item) {
                    HOOK_MALLOC_FAILED;
                    goto handle_error;
                }
                item->next = results;
                results = item;
                item->esp_netif = NULL;
                item->ttl = _str_null_or_empty(instance) ? MDNS_ANSWER_PTR_TTL : MDNS_ANSWER_SRV_TTL;
                item->ip_protocol = MDNS_IP_PROTOCOL_MAX;
                if (srv->instance) {
                    item->instance_name = mdns_mem_strndup(srv->instance, MDNS_NAME_BUF_LEN - 1);
                    if (!item->instance_name) {
                        HOOK_MALLOC_FAILED;
                        goto handle_error;
                    }
                } else {
                    item->instance_name = NULL;
                }
                item->service_type = mdns_mem_strndup(srv->service, MDNS_NAME_BUF_LEN - 1);
                if (!item->service_type) {
                    HOOK_MALLOC_FAILED;
                    goto handle_error;
                }
                item->proto = mdns_mem_strndup(srv->proto, MDNS_NAME_BUF_LEN - 1);
                if (!item->proto) {
                    HOOK_MALLOC_FAILED;
                    goto handle_error;
                }
                item->hostname = mdns_mem_strndup(srv->hostname, MDNS_NAME_BUF_LEN - 1);
                if (!item->hostname) {
                    HOOK_MALLOC_FAILED;
                    goto handle_error;
                }
                item->port = srv->port;
                item->txt = _copy_mdns_txt_items(srv->txt, &(item->txt_value_len), &(item->txt_count));
                // We should not append addresses for selfhost lookup result as we don't know which interface's address to append.
                if (selfhost) {
                    item->addr = NULL;
                } else {
                    item->addr = _copy_delegated_host_address_list(item->hostname);
                    if (!item->addr) {
                        goto handle_error;
                    }
                }
                if (num_results < max_results) {
                    num_results++;
                }
                if (num_results >= max_results) {
                    break;
                }
            }
        }
        s = s->next;
    }
    return results;
handle_error:
    mdns_priv_query_results_free(results);
    return NULL;
}

esp_err_t mdns_service_port_set_for_host(const char *instance, const char *service, const char *proto, const char *host, uint16_t port)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) && port,
                      ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");
    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    s->service->port = port;
    _mdns_announce_all_pcbs(&s, 1, true);

err:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_service_port_set(const char *service, const char *proto, uint16_t port)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_port_set_for_host(NULL, service, proto, NULL, port);
}

esp_err_t mdns_service_txt_set_for_host(const char *instance, const char *service, const char *proto, const char *host,
                                        mdns_txt_item_t txt_items[], uint8_t num_items)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) && !(num_items && txt_items == NULL),
                      ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");
    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    mdns_txt_linked_item_t *new_txt = NULL;
    if (num_items) {
        new_txt = _mdns_allocate_txt(num_items, txt_items);
        if (!new_txt) {
            return ESP_ERR_NO_MEM;
        }
    }
    mdns_service_t *srv = s->service;
    mdns_txt_linked_item_t *txt = srv->txt;
    srv->txt = NULL;
    _mdns_free_linked_txt(txt);
    srv->txt = new_txt;
    _mdns_announce_all_pcbs(&s, 1, false);

err:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_service_txt_set(const char *service, const char *proto, mdns_txt_item_t txt[], uint8_t num_items)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_txt_set_for_host(NULL, service, proto, NULL, txt, num_items);
}

esp_err_t mdns_service_txt_item_set_for_host_with_explicit_value_len(const char *instance, const char *service, const char *proto,
                                                                     const char *host, const char *key, const char *value_arg, uint8_t value_len)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    char *value = NULL;
    mdns_txt_linked_item_t *new_txt = NULL;
    const char *hostname = host ? host : _mdns_server->hostname;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) && !_str_null_or_empty(key) &&
                      !((!value_arg && value_len)), ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");

    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    mdns_service_t *srv = s->service;
    if (value_len > 0) {
        value = (char *) mdns_mem_malloc(value_len);
        ESP_GOTO_ON_FALSE(value, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
        memcpy(value, value_arg, value_len);
    } else {
        value_len = 0;
    }
    mdns_txt_linked_item_t *txt = srv->txt;
    while (txt) {
        if (strcmp(txt->key, key) == 0) {
            mdns_mem_free((char *)txt->value);
            txt->value = value;
            txt->value_len = value_len;
            break;
        }
        txt = txt->next;
    }
    if (!txt) {
        new_txt = (mdns_txt_linked_item_t *)mdns_mem_malloc(sizeof(mdns_txt_linked_item_t));
        ESP_GOTO_ON_FALSE(new_txt, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
        new_txt->key = mdns_mem_strdup(key);
        ESP_GOTO_ON_FALSE(new_txt->key, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
        new_txt->value = value;
        new_txt->value_len = value_len;
        new_txt->next = srv->txt;
        srv->txt = new_txt;
    }

    _mdns_announce_all_pcbs(&s, 1, false);

err:
    MDNS_SERVICE_UNLOCK();
    return ret;
out_of_mem:
    MDNS_SERVICE_UNLOCK();
    HOOK_MALLOC_FAILED;
    mdns_mem_free(value);
    mdns_mem_free(new_txt);
    return ret;
}

esp_err_t mdns_service_txt_item_set_for_host(const char *instance, const char *service, const char *proto, const char *hostname,
                                             const char *key, const char *value)
{
    return mdns_service_txt_item_set_for_host_with_explicit_value_len(instance, service, proto, hostname, key, value,
                                                                      strlen(value));
}


esp_err_t mdns_service_txt_item_set(const char *service, const char *proto, const char *key, const char *value)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_txt_item_set_for_host_with_explicit_value_len(NULL, service, proto, NULL, key,
                                                                      value, strlen(value));
}

esp_err_t mdns_service_txt_item_set_with_explicit_value_len(const char *service, const char *proto, const char *key,
                                                            const char *value, uint8_t value_len)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_txt_item_set_for_host_with_explicit_value_len(NULL, service, proto, NULL, key, value, value_len);
}

esp_err_t mdns_service_txt_item_remove_for_host(const char *instance, const char *service, const char *proto, const char *host,
                                                const char *key)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) && !_str_null_or_empty(key),
                      ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");

    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    mdns_service_t *srv = s->service;
    mdns_txt_linked_item_t *txt = srv->txt;
    if (!txt) {
        goto err;
    }
    if (strcmp(txt->key, key) == 0) {
        srv->txt = txt->next;
        mdns_mem_free((char *)txt->key);
        mdns_mem_free((char *)txt->value);
        mdns_mem_free(txt);
    } else {
        while (txt->next) {
            if (strcmp(txt->next->key, key) == 0) {
                mdns_txt_linked_item_t *t = txt->next;
                txt->next = t->next;
                mdns_mem_free((char *)t->key);
                mdns_mem_free((char *)t->value);
                mdns_mem_free(t);
                break;
            } else {
                txt = txt->next;
            }
        }
    }

    _mdns_announce_all_pcbs(&s, 1, false);

err:
    MDNS_SERVICE_UNLOCK();
    if (ret == ESP_ERR_NO_MEM) {
        HOOK_MALLOC_FAILED;
    }
    return ret;
}

esp_err_t mdns_service_txt_item_remove(const char *service, const char *proto, const char *key)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_txt_item_remove_for_host(NULL, service, proto, NULL, key);
}

static esp_err_t _mdns_service_subtype_remove_for_host(mdns_srv_item_t *service, const char *subtype)
{
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    mdns_subtype_t *srv_subtype = service->service->subtype;
    mdns_subtype_t *pre = service->service->subtype;
    while (srv_subtype) {
        if (strcmp(srv_subtype->subtype, subtype) == 0) {
            // Target subtype is found.
            if (srv_subtype == service->service->subtype) {
                // The first node needs to be removed
                service->service->subtype = service->service->subtype->next;
            } else {
                pre->next = srv_subtype->next;
            }
            mdns_mem_free((char *)srv_subtype->subtype);
            mdns_mem_free(srv_subtype);
            ret = ESP_OK;
            break;
        }
        pre = srv_subtype;
        srv_subtype = srv_subtype->next;
    }
    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Subtype : %s doesn't exist", subtype);
    }

    return ret;
}

esp_err_t mdns_service_subtype_remove_for_host(const char *instance_name, const char *service, const char *proto,
                                               const char *hostname, const char *subtype)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) &&
                      !_str_null_or_empty(subtype), ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");

    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance_name, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    ret = _mdns_service_subtype_remove_for_host(s, subtype);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to remove the subtype: %s", subtype);

    // Transmit a sendbye message for the removed subtype.
    mdns_subtype_t *remove_subtypes = (mdns_subtype_t *)mdns_mem_malloc(sizeof(mdns_subtype_t));
    ESP_GOTO_ON_FALSE(remove_subtypes, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
    remove_subtypes->subtype = mdns_mem_strdup(subtype);
    ESP_GOTO_ON_FALSE(remove_subtypes->subtype, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
    remove_subtypes->next = NULL;

    _mdns_send_bye_subtype(s, instance_name, remove_subtypes);
    _mdns_free_subtype(remove_subtypes);
err:
    MDNS_SERVICE_UNLOCK();
    return ret;
out_of_mem:
    HOOK_MALLOC_FAILED;
    mdns_mem_free(remove_subtypes);
    MDNS_SERVICE_UNLOCK();
    return ret;
}

static esp_err_t _mdns_service_subtype_add_for_host(mdns_srv_item_t *service, const char *subtype)
{
    esp_err_t ret = ESP_OK;
    mdns_subtype_t *srv_subtype = service->service->subtype;
    while (srv_subtype) {
        ESP_GOTO_ON_FALSE(strcmp(srv_subtype->subtype, subtype) != 0, ESP_ERR_INVALID_ARG, err, TAG, "Subtype: %s has already been added", subtype);
        srv_subtype = srv_subtype->next;
    }

    mdns_subtype_t *subtype_item = (mdns_subtype_t *)mdns_mem_malloc(sizeof(mdns_subtype_t));
    ESP_GOTO_ON_FALSE(subtype_item, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
    subtype_item->subtype = mdns_mem_strdup(subtype);
    ESP_GOTO_ON_FALSE(subtype_item->subtype, ESP_ERR_NO_MEM, out_of_mem, TAG, "Out of memory");
    subtype_item->next = service->service->subtype;
    service->service->subtype = subtype_item;

err:
    return ret;
out_of_mem:
    HOOK_MALLOC_FAILED;
    mdns_mem_free(subtype_item);
    return ret;
}

esp_err_t mdns_service_subtype_add_multiple_items_for_host(const char *instance_name, const char *service, const char *proto,
                                                           const char *hostname, mdns_subtype_item_t subtype[], uint8_t num_items)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    int cur_index = 0;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) &&
                      (num_items > 0), ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");

    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance_name, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    for (; cur_index < num_items; cur_index++) {
        ret = _mdns_service_subtype_add_for_host(s, subtype[cur_index].subtype);
        if (ret == ESP_OK) {
            continue;
        } else if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE(TAG, "Out of memory");
            goto err;
        } else {
            ESP_LOGE(TAG, "Failed to add subtype: %s", subtype[cur_index].subtype);
            goto exit;
        }
    }

    _mdns_announce_all_pcbs(&s, 1, false);
err:
    if (ret == ESP_ERR_NO_MEM) {
        for (int idx = 0; idx < cur_index; idx++) {
            _mdns_service_subtype_remove_for_host(s, subtype[idx].subtype);
        }
    }
exit:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_service_subtype_add_for_host(const char *instance_name, const char *service_type, const char *proto,
                                            const char *hostname, const char *subtype)
{
    mdns_subtype_item_t _subtype[1];
    _subtype[0].subtype = subtype;
    return mdns_service_subtype_add_multiple_items_for_host(instance_name, service_type, proto, hostname, _subtype, 1);
}

static mdns_subtype_t *_mdns_service_find_subtype_needed_sendbye(mdns_service_t *service, mdns_subtype_item_t subtype[],
                                                                 uint8_t num_items)
{
    if (!service) {
        return NULL;
    }

    mdns_subtype_t *current = service->subtype;
    mdns_subtype_t *prev = NULL;
    mdns_subtype_t *prev_goodbye = NULL;
    mdns_subtype_t *out_goodbye_subtype = NULL;

    while (current) {
        bool subtype_in_update = false;

        for (int i = 0; i < num_items; i++) {
            if (strcmp(subtype[i].subtype, current->subtype) == 0) {
                subtype_in_update = true;
                break;
            }
        }

        if (!subtype_in_update) {
            // Remove from original list
            if (prev) {
                prev->next = current->next;
            } else {
                service->subtype = current->next;
            }

            mdns_subtype_t *to_move = current;
            current = current->next;

            // Add to goodbye list
            to_move->next = NULL;
            if (prev_goodbye) {
                prev_goodbye->next = to_move;
            } else {
                out_goodbye_subtype = to_move;
            }
            prev_goodbye = to_move;
        } else {
            prev = current;
            current = current->next;
        }
    }

    return out_goodbye_subtype;
}

esp_err_t mdns_service_subtype_update_multiple_items_for_host(const char *instance_name, const char *service_type, const char *proto,
                                                              const char *hostname, mdns_subtype_item_t subtype[], uint8_t num_items)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    int cur_index = 0;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service_type) && !_str_null_or_empty(proto),
                      ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");

    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance_name, service_type, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    mdns_subtype_t *goodbye_subtype = _mdns_service_find_subtype_needed_sendbye(s->service, subtype, num_items);

    if (goodbye_subtype) {
        _mdns_send_bye_subtype(s, instance_name, goodbye_subtype);
    }

    _mdns_free_subtype(goodbye_subtype);
    _mdns_free_service_subtype(s->service);

    for (; cur_index < num_items; cur_index++) {
        ret = _mdns_service_subtype_add_for_host(s, subtype[cur_index].subtype);
        if (ret == ESP_OK) {
            continue;
        } else if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE(TAG, "Out of memory");
            goto err;
        } else {
            ESP_LOGE(TAG, "Failed to add subtype: %s", subtype[cur_index].subtype);
            goto exit;
        }
    }
    if (num_items) {
        _mdns_announce_all_pcbs(&s, 1, false);
    }
err:
    if (ret == ESP_ERR_NO_MEM) {
        for (int idx = 0; idx < cur_index; idx++) {
            _mdns_service_subtype_remove_for_host(s, subtype[idx].subtype);
        }
    }
exit:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_service_instance_name_set_for_host(const char *instance_old, const char *service, const char *proto, const char *host,
                                                  const char *instance)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;

    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) &&
                      !_str_null_or_empty(instance) && strlen(instance) <= (MDNS_NAME_BUF_LEN - 1), ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");

    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance_old, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    if (s->service->instance) {
        mdns_responder_send_bye_service(&s, 1, false);
        mdns_mem_free((char *)s->service->instance);
    }
    s->service->instance = mdns_mem_strndup(instance, MDNS_NAME_BUF_LEN - 1);
    ESP_GOTO_ON_FALSE(s->service->instance, ESP_ERR_NO_MEM, err, TAG, "Out of memory");
    mdns_responder_probe_all_pcbs(&s, 1, false, false);

err:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_service_instance_name_set(const char *service, const char *proto, const char *instance)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_instance_name_set_for_host(NULL, service, proto, NULL, instance);
}

esp_err_t mdns_service_remove_for_host(const char *instance, const char *service, const char *proto, const char *host)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto),
                      ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");
    mdns_srv_item_t *s = mdns_utils_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    mdns_srv_item_t *a = _mdns_server->services;
    mdns_srv_item_t *b = a;
    if (instance) {
        while (a) {
            if (mdns_utils_service_match_instance(a->service, instance, service, proto, hostname)) {
                if (_mdns_server->services != a) {
                    b->next = a->next;
                } else {
                    _mdns_server->services = a->next;
                }
                mdns_responder_send_bye_service(&a, 1, false);
                _mdns_remove_scheduled_service_packets(a->service);
                _mdns_free_service(a->service);
                mdns_mem_free(a);
                break;
            }
            b = a;
            a = a->next;
        }
    } else {
        while (a) {
            if (mdns_utils_service_match(a->service, service, proto, hostname)) {
                if (_mdns_server->services != a) {
                    b->next = a->next;
                } else {
                    _mdns_server->services = a->next;
                }
                mdns_responder_send_bye_service(&a, 1, false);
                _mdns_remove_scheduled_service_packets(a->service);
                _mdns_free_service(a->service);
                mdns_mem_free(a);
                break;
            }
            b = a;
            a = a->next;
        }
    }

err:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_service_remove(const char *service_type, const char *proto)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    return mdns_service_remove_for_host(NULL, service_type, proto, NULL);
}

esp_err_t mdns_service_remove_all(void)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(_mdns_server, ESP_ERR_INVALID_ARG, done, TAG, "Invalid state");
    if (!_mdns_server->services) {
        goto done;
    }

    _mdns_send_final_bye(false);
    mdns_srv_item_t *services = _mdns_server->services;
    _mdns_server->services = NULL;
    while (services) {
        mdns_srv_item_t *s = services;
        services = services->next;
        _mdns_remove_scheduled_service_packets(s->service);
        _mdns_free_service(s->service);
        mdns_mem_free(s);
    }

done:
    MDNS_SERVICE_UNLOCK();
    return ret;
}

esp_err_t mdns_lookup_delegated_service(const char *instance, const char *service, const char *proto, size_t max_results,
                                        mdns_result_t **result)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!result || _str_null_or_empty(service) || _str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }
    MDNS_SERVICE_LOCK();
    *result = _mdns_lookup_service(instance, service, proto, max_results, false);
    MDNS_SERVICE_UNLOCK();
    return ESP_OK;
}

esp_err_t mdns_lookup_selfhosted_service(const char *instance, const char *service, const char *proto, size_t max_results,
                                         mdns_result_t **result)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!result || _str_null_or_empty(service) || _str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }
    MDNS_SERVICE_LOCK();
    *result = _mdns_lookup_service(instance, service, proto, max_results, true);
    MDNS_SERVICE_UNLOCK();
    return ESP_OK;
}
