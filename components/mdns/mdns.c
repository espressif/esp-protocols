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
#include "esp_event.h"
#include "esp_random.h"
#include "esp_check.h"
#include "mdns.h"
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_mem_caps.h"
#include "mdns_utils.h"
#include "mdns_debug.h"
#include "mdns_browse.h"
#include "mdns_netif.h"
#include "mdns_send.h"
#include "mdns_querier.h"


#if CONFIG_ETH_ENABLED && CONFIG_MDNS_PREDEF_NETIF_ETH
#include "esp_eth.h"
#endif

#if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(5, 1, 0)
#define MDNS_ESP_WIFI_ENABLED CONFIG_SOC_WIFI_SUPPORTED
#else
#define MDNS_ESP_WIFI_ENABLED CONFIG_ESP_WIFI_ENABLED
#endif

#if MDNS_ESP_WIFI_ENABLED && (CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP)
#include "esp_wifi.h"
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

// Internal size of IPv6 address is defined here as size of AAAA record in mdns packet
// since the ip6_addr_t is defined in lwip and depends on using IPv6 zones
#define _MDNS_SIZEOF_IP6_ADDR (MDNS_ANSWER_AAAA_SIZE)

//static const char *MDNS_DEFAULT_DOMAIN = "local";
//static const char *MDNS_SUB_STR = "_sub";

mdns_server_t *_mdns_server = NULL;
static mdns_host_item_t *_mdns_host_list = NULL;
static mdns_host_item_t _mdns_self_host;

static const char *TAG = "mdns";

static volatile TaskHandle_t _mdns_service_task_handle = NULL;
static SemaphoreHandle_t _mdns_service_semaphore = NULL;
static StackType_t *_mdns_stack_buffer;


//static bool _mdns_append_host_list(mdns_out_answer_t **destination, bool flush, bool bye);

static esp_err_t mdns_post_custom_action_tcpip_if(mdns_if_t mdns_if, mdns_event_actions_t event_action);

//static void _mdns_query_results_free(mdns_result_t *results);

typedef enum {
    MDNS_IF_STA = 0,
    MDNS_IF_AP = 1,
    MDNS_IF_ETH = 2,
} mdns_predef_if_t;

typedef struct mdns_interfaces mdns_interfaces_t;

struct mdns_interfaces {
    const bool predefined;
    esp_netif_t *netif;
    const mdns_predef_if_t predef_if;
    mdns_if_t duplicate;
};

/*
 * @brief  Internal collection of mdns supported interfaces
 *
 */
static mdns_interfaces_t s_esp_netifs[MDNS_MAX_INTERFACES] = {
#if CONFIG_MDNS_PREDEF_NETIF_STA
    { .predefined = true, .netif = NULL, .predef_if = MDNS_IF_STA, .duplicate = MDNS_MAX_INTERFACES },
#endif
#if CONFIG_MDNS_PREDEF_NETIF_AP
    { .predefined = true, .netif = NULL, .predef_if = MDNS_IF_AP,  .duplicate = MDNS_MAX_INTERFACES },
#endif
#if CONFIG_MDNS_PREDEF_NETIF_ETH
    { .predefined = true, .netif = NULL, .predef_if = MDNS_IF_ETH, .duplicate = MDNS_MAX_INTERFACES },
#endif
};


/**
 * @brief  Convert Predefined interface to the netif id from the internal netif list
 * @param  predef_if Predefined interface enum
 * @return Ordinal number of internal list of mdns network interface.
 *         Returns MDNS_MAX_INTERFACES if the predefined interface wasn't found in the list
 */
static mdns_if_t mdns_if_from_preset_if(mdns_predef_if_t predef_if)
{
    for (int i = 0; i < MDNS_MAX_INTERFACES; ++i) {
        if (s_esp_netifs[i].predefined && s_esp_netifs[i].predef_if == predef_if) {
            return i;
        }
    }
    return MDNS_MAX_INTERFACES;
}

/**
 * @brief  Convert Predefined interface to esp-netif handle
 * @param  predef_if Predefined interface enum
 * @return esp_netif pointer from system list of network interfaces
 */
static inline esp_netif_t *esp_netif_from_preset_if(mdns_predef_if_t predef_if)
{
    switch (predef_if) {
    case MDNS_IF_STA:
        return esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    case MDNS_IF_AP:
        return esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
#if CONFIG_ETH_ENABLED && CONFIG_MDNS_PREDEF_NETIF_ETH
    case MDNS_IF_ETH:
        return esp_netif_get_handle_from_ifkey("ETH_DEF");
#endif
    default:
        return NULL;
    }
}

/**
 * @brief Gets the actual esp_netif pointer from the internal network interface list
 *
 * The supplied ordinal number could
 * - point to a predef netif -> "STA", "AP", "ETH"
 *      - if no entry in the list (NULL) -> check if the system added this netif
 * - point to a custom netif -> just return the entry in the list
 *      - users is responsible for the lifetime of this netif (to be valid between mdns-init -> deinit)
 *
 * @param tcpip_if Ordinal number of the interface
 * @return Pointer ot the esp_netif object if the interface is available, NULL otherwise
 */
esp_netif_t *_mdns_get_esp_netif(mdns_if_t tcpip_if)
{
    if (tcpip_if < MDNS_MAX_INTERFACES) {
        if (s_esp_netifs[tcpip_if].netif == NULL && s_esp_netifs[tcpip_if].predefined) {
            // If the local copy is NULL and this netif is predefined -> we can find it in the global netif list
            s_esp_netifs[tcpip_if].netif = esp_netif_from_preset_if(s_esp_netifs[tcpip_if].predef_if);
            // failing to find it means that the netif is *not* available -> return NULL
        }
        return s_esp_netifs[tcpip_if].netif;
    }
    return NULL;
}


/*
 * @brief Clean internal mdns interface's pointer
 */
static inline void _mdns_clean_netif_ptr(mdns_if_t tcpip_if)
{
    if (tcpip_if < MDNS_MAX_INTERFACES) {
        s_esp_netifs[tcpip_if].netif = NULL;
    }
}


/*
 * @brief  Convert esp-netif handle to mdns if
 */
static mdns_if_t _mdns_get_if_from_esp_netif(esp_netif_t *esp_netif)
{
    for (int i = 0; i < MDNS_MAX_INTERFACES; ++i) {
        // The predefined netifs in the static array are NULL when firstly calling this function
        // if IPv4 is disabled. Set these netifs here.
        if (s_esp_netifs[i].netif == NULL && s_esp_netifs[i].predefined) {
            s_esp_netifs[i].netif = esp_netif_from_preset_if(s_esp_netifs[i].predef_if);
        }
        if (esp_netif == s_esp_netifs[i].netif) {
            return i;
        }
    }
    return MDNS_MAX_INTERFACES;
}

/**
 * @brief  Check if interface is duplicate (two interfaces on the same subnet)
 */
bool _mdns_if_is_dup(mdns_if_t tcpip_if)
{
    mdns_if_t other_if = _mdns_get_other_if(tcpip_if);
    if (other_if == MDNS_MAX_INTERFACES) {
        return false;
    }
    if (_mdns_server->interfaces[tcpip_if].pcbs[MDNS_IP_PROTOCOL_V4].state == PCB_DUP
            || _mdns_server->interfaces[tcpip_if].pcbs[MDNS_IP_PROTOCOL_V6].state == PCB_DUP
            || _mdns_server->interfaces[other_if].pcbs[MDNS_IP_PROTOCOL_V4].state == PCB_DUP
            || _mdns_server->interfaces[other_if].pcbs[MDNS_IP_PROTOCOL_V6].state == PCB_DUP
       ) {
        return true;
    }
    return false;
}

const char *mdns_utils_get_global_hostname(void)
{
    return _mdns_server ? _mdns_server->hostname : NULL;
}

mdns_srv_item_t *mdns_utils_get_services(void)
{
    return _mdns_server->services;
}

mdns_host_item_t *mdns_utils_get_hosts(void)
{
    return _mdns_host_list;
}

mdns_host_item_t *mdns_utils_get_selfhost(void)
{
    return &_mdns_self_host;
}


void mdns_utils_set_global_hostname(const char *hostname)
{
    if (_mdns_server) {
        if (_mdns_server->hostname) {
            mdns_mem_free((void *)_mdns_server->hostname);
        }
        _mdns_server->hostname = hostname;
        _mdns_self_host.hostname = hostname;
    }
}

const char *mdns_utils_get_instance(void)
{
    return _mdns_server ? _mdns_server->instance : NULL;
}

void mdns_utils_set_instance(const char *instance)
{
    if (_mdns_server) {
        if (_mdns_server->instance) {
            mdns_mem_free((void *)_mdns_server->instance);
        }
        _mdns_server->instance = instance;
    }
}

mdns_search_once_t *mdns_utils_get_search(void)
{
    return _mdns_server->search_once;
}

mdns_browse_t *mdns_utils_get_browse(void)
{
    return _mdns_server->browse;
}

bool is_mdns_server(void)
{
    return _mdns_server != NULL;
}

static inline bool _str_null_or_empty(const char *str)
{
    return (str == NULL || *str == 0);
}

mdns_tx_packet_t *mdns_utils_get_tx_packet(void)
{
    return _mdns_server->tx_queue_head;
}

bool mdns_utils_is_pcb_after_init(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    return _mdns_server->interfaces[tcpip_if].pcbs[ip_protocol].state > PCB_INIT;
}

bool mdns_utils_is_pcb_duplicate(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    return _mdns_server->interfaces[tcpip_if].pcbs[ip_protocol].state == PCB_DUP;
}

bool mdns_utils_is_probing(mdns_rx_packet_t *packet)
{
    return _mdns_server->interfaces[packet->tcpip_if].pcbs[packet->ip_protocol].probe_running;
}

bool mdns_utils_after_probing(mdns_rx_packet_t *packet)
{
    return _mdns_server->interfaces[packet->tcpip_if].pcbs[packet->ip_protocol].state > PCB_PROBE_3;
}

void mdns_utils_probe_failed(mdns_rx_packet_t *packet)
{
    _mdns_server->interfaces[packet->tcpip_if].pcbs[packet->ip_protocol].failed_probes++;
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

esp_err_t _mdns_send_rx_action(mdns_rx_packet_t *packet)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = ACTION_RX_HANDLE;
    action->data.rx_handle.packet = packet;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief  Helper to get either ETH or STA if the other is provided
 *          Used when two interfaces are on the same subnet
 */
mdns_if_t _mdns_get_other_if(mdns_if_t tcpip_if)
{
    if (tcpip_if < MDNS_MAX_INTERFACES) {
        return s_esp_netifs[tcpip_if].duplicate;
    }
    return MDNS_MAX_INTERFACES;
}

#ifdef CONFIG_LWIP_IPV6
/**
 * @brief  Check if IPv6 address is NULL
 */
bool _ipv6_address_is_zero(esp_ip6_addr_t ip6)
{
    uint8_t i;
    uint8_t *data = (uint8_t *)ip6.addr;
    for (i = 0; i < _MDNS_SIZEOF_IP6_ADDR; i++) {
        if (data[i]) {
            return false;
        }
    }
    return true;
}
#endif /* CONFIG_LWIP_IPV6 */












/**
 * @brief  schedules a packet to be sent after given milliseconds
 *
 * @param  packet       the packet
 * @param  ms_after     number of milliseconds after which the packet should be dispatched
 */
void _mdns_schedule_tx_packet(mdns_tx_packet_t *packet, uint32_t ms_after)
{
    if (!packet) {
        return;
    }
    packet->send_at = (xTaskGetTickCount() * portTICK_PERIOD_MS) + ms_after;
    packet->next = NULL;
    if (!_mdns_server->tx_queue_head || _mdns_server->tx_queue_head->send_at > packet->send_at) {
        packet->next = _mdns_server->tx_queue_head;
        _mdns_server->tx_queue_head = packet;
        return;
    }
    mdns_tx_packet_t *q = _mdns_server->tx_queue_head;
    while (q->next && q->next->send_at <= packet->send_at) {
        q = q->next;
    }
    packet->next = q->next;
    q->next = packet;
}

/**
 * @brief  free all packets scheduled for sending
 */
static void _mdns_clear_tx_queue_head(void)
{
    mdns_tx_packet_t *q;
    while (_mdns_server->tx_queue_head) {
        q = _mdns_server->tx_queue_head;
        _mdns_server->tx_queue_head = _mdns_server->tx_queue_head->next;
        _mdns_free_tx_packet(q);
    }
}

/**
 * @brief  clear packets scheduled for sending on a specific interface
 *
 * @param  tcpip_if     the interface
 * @param  ip_protocol     pcb type V4/V6
 */
static void _mdns_clear_pcb_tx_queue_head(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_tx_packet_t *q, * p;
    while (_mdns_server->tx_queue_head && _mdns_server->tx_queue_head->tcpip_if == tcpip_if && _mdns_server->tx_queue_head->ip_protocol == ip_protocol) {
        q = _mdns_server->tx_queue_head;
        _mdns_server->tx_queue_head = _mdns_server->tx_queue_head->next;
        _mdns_free_tx_packet(q);
    }
    if (_mdns_server->tx_queue_head) {
        q = _mdns_server->tx_queue_head;
        while (q->next) {
            if (q->next->tcpip_if == tcpip_if && q->next->ip_protocol == ip_protocol) {
                p = q->next;
                q->next = p->next;
                _mdns_free_tx_packet(p);
            } else {
                q = q->next;
            }
        }
    }
}

/**
 * @brief  get the next packet scheduled for sending on a specific interface
 *
 * @param  tcpip_if     the interface
 * @param  ip_protocol     pcb type V4/V6
 */
static mdns_tx_packet_t *_mdns_get_next_pcb_packet(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_tx_packet_t *q = _mdns_server->tx_queue_head;
    while (q) {
        if (q->tcpip_if == tcpip_if && q->ip_protocol == ip_protocol) {
            return q;
        }
        q = q->next;
    }
    return NULL;
}


/**
 * @brief  Remove and free answer from answer list (destination)
 */
static void _mdns_dealloc_answer(mdns_out_answer_t **destination, uint16_t type, mdns_srv_item_t *service)
{
    mdns_out_answer_t *d = *destination;
    if (!d) {
        return;
    }
    mdns_srv_item_t s = {NULL, NULL};
    if (!service) {
        service = &s;
    }
    if (d->type == type && d->service == service->service) {
        *destination = d->next;
        mdns_mem_free(d);
        return;
    }
    while (d->next) {
        mdns_out_answer_t *a = d->next;
        if (a->type == type && a->service == service->service) {
            d->next = a->next;
            mdns_mem_free(a);
            return;
        }
        d = d->next;
    }
}











/**
 * @brief  Send probe for additional services on particular PCB
 */
static void _mdns_init_pcb_probe_new_service(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool probe_ip)
{
    mdns_pcb_t *pcb = &_mdns_server->interfaces[tcpip_if].pcbs[ip_protocol];
    size_t services_final_len = len;

    if (PCB_STATE_IS_PROBING(pcb)) {
        services_final_len += pcb->probe_services_len;
    }
    mdns_srv_item_t **_services = NULL;
    if (services_final_len) {
        _services = (mdns_srv_item_t **)mdns_mem_malloc(sizeof(mdns_srv_item_t *) * services_final_len);
        if (!_services) {
            HOOK_MALLOC_FAILED;
            return;
        }

        size_t i;
        for (i = 0; i < len; i++) {
            _services[i] = services[i];
        }
        if (pcb->probe_services) {
            for (i = 0; i < pcb->probe_services_len; i++) {
                _services[len + i] = pcb->probe_services[i];
            }
            mdns_mem_free(pcb->probe_services);
        }
    }

    probe_ip = pcb->probe_ip || probe_ip;

    pcb->probe_ip = false;
    pcb->probe_services = NULL;
    pcb->probe_services_len = 0;
    pcb->probe_running = false;

    mdns_tx_packet_t *packet = _mdns_create_probe_packet(tcpip_if, ip_protocol, _services, services_final_len, true, probe_ip);
    if (!packet) {
        mdns_mem_free(_services);
        return;
    }

    pcb->probe_ip = probe_ip;
    pcb->probe_services = _services;
    pcb->probe_services_len = services_final_len;
    pcb->probe_running = true;
    _mdns_schedule_tx_packet(packet, ((pcb->failed_probes > 5) ? 1000 : 120) + (esp_random() & 0x7F));
    pcb->state = PCB_PROBE_1;
}

/**
 * @brief  Send probe for particular services on particular PCB
 *
 * Tests possible duplication on probing service structure and probes only for new entries.
 * - If pcb probing then add only non-probing services and restarts probing
 * - If pcb not probing, run probing for all specified services
 */
void _mdns_init_pcb_probe(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool probe_ip)
{
    mdns_pcb_t *pcb = &_mdns_server->interfaces[tcpip_if].pcbs[ip_protocol];

    _mdns_clear_pcb_tx_queue_head(tcpip_if, ip_protocol);

    if (_str_null_or_empty(_mdns_server->hostname)) {
        pcb->state = PCB_RUNNING;
        return;
    }

    if (PCB_STATE_IS_PROBING(pcb)) {
        // Looking for already probing services to resolve duplications
        mdns_srv_item_t *new_probe_services[len];
        int new_probe_service_len = 0;
        bool found;
        for (size_t j = 0; j < len; ++j) {
            found = false;
            for (int i = 0; i < pcb->probe_services_len; ++i) {
                if (pcb->probe_services[i] == services[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                new_probe_services[new_probe_service_len++] = services[j];
            }
        }
        // init probing for newly added services
        _mdns_init_pcb_probe_new_service(tcpip_if, ip_protocol,
                                         new_probe_service_len ? new_probe_services : NULL, new_probe_service_len, probe_ip);
    } else {
        // not probing, so init for all services
        _mdns_init_pcb_probe_new_service(tcpip_if, ip_protocol, services, len, probe_ip);
    }
}

/**
 * @brief  Restart the responder on particular PCB
 */
static void _mdns_restart_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    size_t srv_count = 0;
    mdns_srv_item_t *a = _mdns_server->services;
    while (a) {
        srv_count++;
        a = a->next;
    }
    if (srv_count == 0) {
        // proble only IP
        _mdns_init_pcb_probe(tcpip_if, ip_protocol, NULL, 0, true);
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t i = 0;
    a = _mdns_server->services;
    while (a) {
        services[i++] = a;
        a = a->next;
    }
    _mdns_init_pcb_probe(tcpip_if, ip_protocol, services, srv_count, true);
}

/**
 * @brief  Send by for particular services
 */
static void _mdns_send_bye(mdns_srv_item_t **services, size_t len, bool include_ip)
{
    uint8_t i, j;
    if (_str_null_or_empty(_mdns_server->hostname)) {
        return;
    }

    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            if (mdns_is_netif_ready(i, j) && _mdns_server->interfaces[i].pcbs[j].state == PCB_RUNNING) {
                _mdns_pcb_send_bye((mdns_if_t)i, (mdns_ip_protocol_t)j, services, len, include_ip);
            }
        }
    }
}



/**
 * @brief  Send announcement on particular PCB
 */
static void _mdns_announce_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool include_ip)
{
    mdns_pcb_t *_pcb = &_mdns_server->interfaces[tcpip_if].pcbs[ip_protocol];
    size_t i;
    if (mdns_is_netif_ready(tcpip_if, ip_protocol)) {
        if (PCB_STATE_IS_PROBING(_pcb)) {
            _mdns_init_pcb_probe(tcpip_if, ip_protocol, services, len, include_ip);
        } else if (PCB_STATE_IS_ANNOUNCING(_pcb)) {
            mdns_tx_packet_t   *p = _mdns_get_next_pcb_packet(tcpip_if, ip_protocol);
            if (p) {
                for (i = 0; i < len; i++) {
                    if (!_mdns_alloc_answer(&p->answers, MDNS_TYPE_SDPTR, services[i]->service, NULL, false, false)
                            || !_mdns_alloc_answer(&p->answers, MDNS_TYPE_PTR, services[i]->service, NULL, false, false)
                            || !_mdns_alloc_answer(&p->answers, MDNS_TYPE_SRV, services[i]->service, NULL, true, false)
                            || !_mdns_alloc_answer(&p->answers, MDNS_TYPE_TXT, services[i]->service, NULL, true, false)) {
                        break;
                    }
                }
                if (include_ip) {
                    _mdns_dealloc_answer(&p->additional, MDNS_TYPE_A, NULL);
                    _mdns_dealloc_answer(&p->additional, MDNS_TYPE_AAAA, NULL);
                    _mdns_append_host_list_in_services(&p->answers, services, len, true, false);
                }
                _pcb->state = PCB_ANNOUNCE_1;
            }
        } else if (_pcb->state == PCB_RUNNING) {

            if (_str_null_or_empty(_mdns_server->hostname)) {
                return;
            }

            _pcb->state = PCB_ANNOUNCE_1;
            mdns_tx_packet_t *p = _mdns_create_announce_packet(tcpip_if, ip_protocol, services, len, include_ip);
            if (p) {
                _mdns_schedule_tx_packet(p, 0);
            }
        }
    }
}

/**
 * @brief  Send probe on all active PCBs
 */
void _mdns_probe_all_pcbs(mdns_srv_item_t **services, size_t len, bool probe_ip, bool clear_old_probe)
{
    uint8_t i, j;
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            if (mdns_is_netif_ready(i, j)) {
                mdns_pcb_t *_pcb = &_mdns_server->interfaces[i].pcbs[j];
                if (clear_old_probe) {
                    mdns_mem_free(_pcb->probe_services);
                    _pcb->probe_services = NULL;
                    _pcb->probe_services_len = 0;
                    _pcb->probe_running = false;
                }
                _mdns_init_pcb_probe((mdns_if_t)i, (mdns_ip_protocol_t)j, services, len, probe_ip);
            }
        }
    }
}

/**
 * @brief  Send announcement on all active PCBs
 */
static void _mdns_announce_all_pcbs(mdns_srv_item_t **services, size_t len, bool include_ip)
{
    uint8_t i, j;
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            _mdns_announce_pcb((mdns_if_t)i, (mdns_ip_protocol_t)j, services, len, include_ip);
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
    _mdns_send_bye(services, srv_count, include_ip);
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
    _mdns_send_bye(services, srv_count, include_ip);
}

/**
 * @brief  Restart the responder on all services without instance
 */
void _mdns_restart_all_pcbs_no_instance(void)
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
    _mdns_probe_all_pcbs(services, srv_count, false, true);
}

/**
 * @brief  Restart the responder on all active PCBs
 */
void _mdns_restart_all_pcbs(void)
{
    _mdns_clear_tx_queue_head();
    size_t srv_count = 0;
    mdns_srv_item_t *a = _mdns_server->services;
    while (a) {
        srv_count++;
        a = a->next;
    }
    if (srv_count == 0) {
        _mdns_probe_all_pcbs(NULL, 0, true, true);
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t l = 0;
    a = _mdns_server->services;
    while (a) {
        services[l++] = a;
        a = a->next;
    }

    _mdns_probe_all_pcbs(services, srv_count, true, true);
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

/**
 * @brief  Remove and free service answer from answer list (destination)
 */
static void _mdns_dealloc_scheduled_service_answers(mdns_out_answer_t **destination, mdns_service_t *service)
{
    mdns_out_answer_t *d = *destination;
    if (!d) {
        return;
    }
    while (d && d->service == service) {
        *destination = d->next;
        mdns_mem_free(d);
        d = *destination;
    }
    while (d && d->next) {
        mdns_out_answer_t *a = d->next;
        if (a->service == service) {
            d->next = a->next;
            mdns_mem_free(a);
        } else {
            d = d->next;
        }
    }
}

/**
 * @brief  Find, remove and free answers and scheduled packets for service
 */
static void _mdns_remove_scheduled_service_packets(mdns_service_t *service)
{
    if (!service) {
        return;
    }
    mdns_tx_packet_t *p = NULL;
    mdns_tx_packet_t *q = _mdns_server->tx_queue_head;
    while (q) {
        bool had_answers = (q->answers != NULL);

        _mdns_dealloc_scheduled_service_answers(&(q->answers), service);
        _mdns_dealloc_scheduled_service_answers(&(q->additional), service);
        _mdns_dealloc_scheduled_service_answers(&(q->servers), service);


        mdns_pcb_t *_pcb = &_mdns_server->interfaces[q->tcpip_if].pcbs[q->ip_protocol];
        if (mdns_is_netif_ready(q->tcpip_if, q->ip_protocol)) {
            if (PCB_STATE_IS_PROBING(_pcb)) {
                uint8_t i;
                //check if we are probing this service
                for (i = 0; i < _pcb->probe_services_len; i++) {
                    mdns_srv_item_t *s = _pcb->probe_services[i];
                    if (s->service == service) {
                        break;
                    }
                }
                if (i < _pcb->probe_services_len) {
                    if (_pcb->probe_services_len > 1) {
                        uint8_t n;
                        for (n = (i + 1); n < _pcb->probe_services_len; n++) {
                            _pcb->probe_services[n - 1] = _pcb->probe_services[n];
                        }
                        _pcb->probe_services_len--;
                    } else {
                        _pcb->probe_services_len = 0;
                        mdns_mem_free(_pcb->probe_services);
                        _pcb->probe_services = NULL;
                        if (!_pcb->probe_ip) {
                            _pcb->probe_running = false;
                            _pcb->state = PCB_RUNNING;
                        }
                    }

                    if (q->questions) {
                        mdns_out_question_t *qsn = NULL;
                        mdns_out_question_t *qs = q->questions;
                        if (qs->type == MDNS_TYPE_ANY
                                && qs->service && strcmp(qs->service, service->service) == 0
                                && qs->proto && strcmp(qs->proto, service->proto) == 0) {
                            q->questions = q->questions->next;
                            mdns_mem_free(qs);
                        } else while (qs->next) {
                                qsn = qs->next;
                                if (qsn->type == MDNS_TYPE_ANY
                                        && qsn->service && strcmp(qsn->service, service->service) == 0
                                        && qsn->proto && strcmp(qsn->proto, service->proto) == 0) {
                                    qs->next = qsn->next;
                                    mdns_mem_free(qsn);
                                    break;
                                }
                                qs = qs->next;
                            }
                    }
                }
            } else if (PCB_STATE_IS_ANNOUNCING(_pcb)) {
                //if answers were cleared, set to running
                if (had_answers && q->answers == NULL) {
                    _pcb->state = PCB_RUNNING;
                }
            }
        }

        p = q;
        q = q->next;
        if (!p->questions && !p->answers && !p->additional && !p->servers) {
            queueDetach(mdns_tx_packet_t, _mdns_server->tx_queue_head, p);
            _mdns_free_tx_packet(p);
        }
    }
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


/*
 * Received Packet Handling
 * */




static esp_err_t mdns_pcb_deinit_local(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_proto)
{
    esp_err_t err = _mdns_pcb_deinit(tcpip_if, ip_proto);
    mdns_pcb_t *_pcb = &_mdns_server->interfaces[tcpip_if].pcbs[ip_proto];
    if (_pcb == NULL || err != ESP_OK) {
        return err;
    }
    mdns_mem_free(_pcb->probe_services);
    _pcb->state = PCB_OFF;
    _pcb->probe_ip = false;
    _pcb->probe_services = NULL;
    _pcb->probe_services_len = 0;
    _pcb->probe_running = false;
    _pcb->failed_probes = 0;
    return ESP_OK;
}
/**
 * @brief  Set interface as duplicate if another is found on the same subnet
 */
void _mdns_dup_interface(mdns_if_t tcpip_if)
{
    uint8_t i;
    mdns_if_t other_if = _mdns_get_other_if(tcpip_if);
    if (other_if == MDNS_MAX_INTERFACES) {
        return; // no other interface found
    }
    for (i = 0; i < MDNS_IP_PROTOCOL_MAX; i++) {
        if (mdns_is_netif_ready(other_if, i)) {
            //stop this interface and mark as dup
            if (mdns_is_netif_ready(tcpip_if, i)) {
                _mdns_clear_pcb_tx_queue_head(tcpip_if, i);
                mdns_pcb_deinit_local(tcpip_if, i);
            }
            _mdns_server->interfaces[tcpip_if].pcbs[i].state = PCB_DUP;
            _mdns_announce_pcb(other_if, i, NULL, 0, true);
        }
    }
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
    if (_hostname_is_ours(hostname)) {
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
            free_address_list(host->address_list);
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
        free_address_list(host->address_list);
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
            _mdns_send_bye(&srv, 1, false);
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
            free_address_list(host->address_list);
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



















void mdns_parse_packet(mdns_rx_packet_t *packet);

/**
 * @brief  Enable mDNS interface
 */
void _mdns_enable_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    if (!mdns_is_netif_ready(tcpip_if, ip_protocol)) {
        if (_mdns_pcb_init(tcpip_if, ip_protocol)) {
            _mdns_server->interfaces[tcpip_if].pcbs[ip_protocol].failed_probes = 0;
            return;
        }
    }
    _mdns_restart_pcb(tcpip_if, ip_protocol);
}

/**
 * @brief  Disable mDNS interface
 */
void _mdns_disable_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    _mdns_clean_netif_ptr(tcpip_if);

    if (mdns_is_netif_ready(tcpip_if, ip_protocol)) {
        _mdns_clear_pcb_tx_queue_head(tcpip_if, ip_protocol);
        mdns_pcb_deinit_local(tcpip_if, ip_protocol);
        mdns_if_t other_if = _mdns_get_other_if(tcpip_if);
        if (other_if != MDNS_MAX_INTERFACES && _mdns_server->interfaces[other_if].pcbs[ip_protocol].state == PCB_DUP) {
            _mdns_server->interfaces[other_if].pcbs[ip_protocol].state = PCB_OFF;
            _mdns_enable_pcb(other_if, ip_protocol);
        }
    }
    _mdns_server->interfaces[tcpip_if].pcbs[ip_protocol].state = PCB_OFF;
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
        _mdns_enable_pcb(mdns_if, MDNS_IP_PROTOCOL_V4);
    }
    if (action & MDNS_EVENT_ENABLE_IP6) {
        _mdns_enable_pcb(mdns_if, MDNS_IP_PROTOCOL_V6);
    }
    if (action & MDNS_EVENT_DISABLE_IP4) {
        _mdns_disable_pcb(mdns_if, MDNS_IP_PROTOCOL_V4);
    }
    if (action & MDNS_EVENT_DISABLE_IP6) {
        _mdns_disable_pcb(mdns_if, MDNS_IP_PROTOCOL_V6);
    }
    if (action & MDNS_EVENT_ANNOUNCE_IP4) {
        _mdns_announce_pcb(mdns_if, MDNS_IP_PROTOCOL_V4, NULL, 0, true);
    }
    if (action & MDNS_EVENT_ANNOUNCE_IP6) {
        _mdns_announce_pcb(mdns_if, MDNS_IP_PROTOCOL_V6, NULL, 0, true);
    }

#ifdef CONFIG_MDNS_RESPOND_REVERSE_QUERIES
#ifdef CONFIG_LWIP_IPV4
    if (action & MDNS_EVENT_IP4_REVERSE_LOOKUP) {
        esp_netif_ip_info_t if_ip_info;
        if (esp_netif_get_ip_info(_mdns_get_esp_netif(mdns_if), &if_ip_info) == ESP_OK) {
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
        if (!esp_netif_get_ip6_linklocal(_mdns_get_esp_netif(mdns_if), &addr6) && !_ipv6_address_is_zero(addr6)) {
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

/**
 * @brief  Dispatch interface changes based on system events
 */
static inline void post_mdns_disable_pcb(mdns_predef_if_t preset_if, mdns_ip_protocol_t protocol)
{
    mdns_post_custom_action_tcpip_if(mdns_if_from_preset_if(preset_if), protocol == MDNS_IP_PROTOCOL_V4 ? MDNS_EVENT_DISABLE_IP4 : MDNS_EVENT_DISABLE_IP6);
}

static inline void post_mdns_enable_pcb(mdns_predef_if_t preset_if, mdns_ip_protocol_t protocol)
{
    mdns_post_custom_action_tcpip_if(mdns_if_from_preset_if(preset_if), protocol == MDNS_IP_PROTOCOL_V4 ? MDNS_EVENT_ENABLE_IP4 : MDNS_EVENT_ENABLE_IP6);
}

static inline void post_mdns_announce_pcb(mdns_predef_if_t preset_if, mdns_ip_protocol_t protocol)
{
    mdns_post_custom_action_tcpip_if(mdns_if_from_preset_if(preset_if), protocol == MDNS_IP_PROTOCOL_V4 ? MDNS_EVENT_ANNOUNCE_IP4 : MDNS_EVENT_ANNOUNCE_IP6);
}

#if CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP || CONFIG_MDNS_PREDEF_NETIF_ETH
void mdns_preset_if_handle_system_event(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data)
{
    if (!_mdns_server) {
        return;
    }

    esp_netif_dhcp_status_t dcst;
#if MDNS_ESP_WIFI_ENABLED && (CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP)
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            if (!esp_netif_dhcpc_get_status(esp_netif_from_preset_if(MDNS_IF_STA), &dcst)) {
                if (dcst == ESP_NETIF_DHCP_STOPPED) {
                    post_mdns_enable_pcb(MDNS_IF_STA, MDNS_IP_PROTOCOL_V4);
                }
            }
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            post_mdns_disable_pcb(MDNS_IF_STA, MDNS_IP_PROTOCOL_V4);
            post_mdns_disable_pcb(MDNS_IF_STA, MDNS_IP_PROTOCOL_V6);
            break;
        case WIFI_EVENT_AP_START:
            post_mdns_enable_pcb(MDNS_IF_AP, MDNS_IP_PROTOCOL_V4);
            break;
        case WIFI_EVENT_AP_STOP:
            post_mdns_disable_pcb(MDNS_IF_AP, MDNS_IP_PROTOCOL_V4);
            post_mdns_disable_pcb(MDNS_IF_AP, MDNS_IP_PROTOCOL_V6);
            break;
        default:
            break;
        }
    } else
#endif
#if CONFIG_ETH_ENABLED && CONFIG_MDNS_PREDEF_NETIF_ETH
        if (event_base == ETH_EVENT) {
            switch (event_id) {
            case ETHERNET_EVENT_CONNECTED:
                if (!esp_netif_dhcpc_get_status(esp_netif_from_preset_if(MDNS_IF_ETH), &dcst)) {
                    if (dcst == ESP_NETIF_DHCP_STOPPED) {
                        post_mdns_enable_pcb(MDNS_IF_ETH, MDNS_IP_PROTOCOL_V4);
                    }
                }
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                post_mdns_disable_pcb(MDNS_IF_ETH, MDNS_IP_PROTOCOL_V4);
                post_mdns_disable_pcb(MDNS_IF_ETH, MDNS_IP_PROTOCOL_V6);
                break;
            default:
                break;
            }
        } else
#endif
            if (event_base == IP_EVENT) {
                switch (event_id) {
                case IP_EVENT_STA_GOT_IP:
                    post_mdns_enable_pcb(MDNS_IF_STA, MDNS_IP_PROTOCOL_V4);
                    post_mdns_announce_pcb(MDNS_IF_STA, MDNS_IP_PROTOCOL_V6);
                    break;
#if CONFIG_ETH_ENABLED && CONFIG_MDNS_PREDEF_NETIF_ETH
                case IP_EVENT_ETH_GOT_IP:
                    post_mdns_enable_pcb(MDNS_IF_ETH, MDNS_IP_PROTOCOL_V4);
                    break;
#endif
                case IP_EVENT_GOT_IP6: {
                    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *) event_data;
                    mdns_if_t mdns_if = _mdns_get_if_from_esp_netif(event->esp_netif);
                    if (mdns_if >= MDNS_MAX_INTERFACES) {
                        return;
                    }
                    post_mdns_enable_pcb(mdns_if, MDNS_IP_PROTOCOL_V6);
                    post_mdns_announce_pcb(mdns_if, MDNS_IP_PROTOCOL_V4);
                    mdns_browse_t *browse = _mdns_server->browse;
                    while (browse) {
                        _mdns_browse_send(browse, mdns_if);
                        browse = browse->next;
                    }
                }
                break;
                default:
                    break;
                }
            }
}
#endif /* CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP || CONFIG_MDNS_PREDEF_NETIF_ETH */

/*
 * MDNS Search
 * */

/**
 * @brief  Free search structure (except the results)
 */
static void _mdns_search_free(mdns_search_once_t *search)
{
    mdns_mem_free(search->instance);
    mdns_mem_free(search->service);
    mdns_mem_free(search->proto);
    vSemaphoreDelete(search->done_semaphore);
    mdns_mem_free(search);
}

/**
 * @brief  Allocate new search structure
 */
static mdns_search_once_t *_mdns_search_init(const char *name, const char *service, const char *proto, uint16_t type, bool unicast,
                                             uint32_t timeout, uint8_t max_results, mdns_query_notify_t notifier)
{
    mdns_search_once_t *search = (mdns_search_once_t *)mdns_mem_malloc(sizeof(mdns_search_once_t));
    if (!search) {
        HOOK_MALLOC_FAILED;
        return NULL;
    }
    memset(search, 0, sizeof(mdns_search_once_t));

    search->done_semaphore = xSemaphoreCreateBinary();
    if (!search->done_semaphore) {
        mdns_mem_free(search);
        return NULL;
    }

    if (!_str_null_or_empty(name)) {
        search->instance = mdns_mem_strndup(name, MDNS_NAME_BUF_LEN - 1);
        if (!search->instance) {
            _mdns_search_free(search);
            return NULL;
        }
    }

    if (!_str_null_or_empty(service)) {
        search->service = mdns_mem_strndup(service, MDNS_NAME_BUF_LEN - 1);
        if (!search->service) {
            _mdns_search_free(search);
            return NULL;
        }
    }

    if (!_str_null_or_empty(proto)) {
        search->proto = mdns_mem_strndup(proto, MDNS_NAME_BUF_LEN - 1);
        if (!search->proto) {
            _mdns_search_free(search);
            return NULL;
        }
    }

    search->type = type;
    search->unicast = unicast;
    search->timeout = timeout;
    search->num_results = 0;
    search->max_results = max_results;
    search->result = NULL;
    search->state = SEARCH_INIT;
    search->sent_at = 0;
    search->started_at = xTaskGetTickCount() * portTICK_PERIOD_MS;
    search->notifier = notifier;
    search->next = NULL;

    return search;
}

/**
 * @brief  Mark search as finished and remove it from search chain
 */
void _mdns_search_finish(mdns_search_once_t *search)
{
    search->state = SEARCH_OFF;
    queueDetach(mdns_search_once_t, _mdns_server->search_once, search);
    if (search->notifier) {
        search->notifier(search);
    }
    xSemaphoreGive(search->done_semaphore);
}

/**
 * @brief  Add new search to the search chain
 */
static void _mdns_search_add(mdns_search_once_t *search)
{
    search->next = _mdns_server->search_once;
    _mdns_server->search_once = search;
}


















static void _mdns_tx_handle_packet(mdns_tx_packet_t *p)
{
    mdns_tx_packet_t *a = NULL;
    mdns_out_question_t *q = NULL;
    mdns_pcb_t *pcb = &_mdns_server->interfaces[p->tcpip_if].pcbs[p->ip_protocol];
    uint32_t send_after = 1000;

    if (pcb->state == PCB_OFF) {
        _mdns_free_tx_packet(p);
        return;
    }
    _mdns_dispatch_tx_packet(p);

    switch (pcb->state) {
    case PCB_PROBE_1:
        q = p->questions;
        while (q) {
            q->unicast = false;
            q = q->next;
        }
    //fallthrough
    case PCB_PROBE_2:
        _mdns_schedule_tx_packet(p, 250);
        pcb->state = (mdns_pcb_state_t)((uint8_t)(pcb->state) + 1);
        break;
    case PCB_PROBE_3:
        a = _mdns_create_announce_from_probe(p);
        if (!a) {
            _mdns_schedule_tx_packet(p, 250);
            break;
        }
        pcb->probe_running = false;
        pcb->probe_ip = false;
        pcb->probe_services_len = 0;
        pcb->failed_probes = 0;
        mdns_mem_free(pcb->probe_services);
        pcb->probe_services = NULL;
        _mdns_free_tx_packet(p);
        p = a;
        send_after = 250;
    //fallthrough
    case PCB_ANNOUNCE_1:
    //fallthrough
    case PCB_ANNOUNCE_2:
        _mdns_schedule_tx_packet(p, send_after);
        pcb->state = (mdns_pcb_state_t)((uint8_t)(pcb->state) + 1);
        break;
    case PCB_ANNOUNCE_3:
        pcb->state = PCB_RUNNING;
        _mdns_free_tx_packet(p);
        break;
    default:
        _mdns_free_tx_packet(p);
        break;
    }
}

void _mdns_remap_self_service_hostname(const char *old_hostname, const char *new_hostname)
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

static void _mdns_sync_browse_result_link_free(mdns_browse_sync_t *browse_sync)
{
    mdns_browse_result_sync_t *current = browse_sync->sync_result;
    mdns_browse_result_sync_t *need_free;
    while (current) {
        need_free = current;
        current = current->next;
        mdns_mem_free(need_free);
    }
    mdns_mem_free(browse_sync);
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
    //fallthrough
    case ACTION_SEARCH_SEND:
    //fallthrough
    case ACTION_SEARCH_END:
        _mdns_search_free(action->data.search_add.search);
        break;
    case ACTION_BROWSE_ADD:
    //fallthrough
    case ACTION_BROWSE_END:
        _mdns_browse_item_free(action->data.browse_add.browse);
        break;
    case ACTION_BROWSE_SYNC:
        _mdns_sync_browse_result_link_free(action->data.browse_sync.browse_sync);
        break;
    case ACTION_TX_HANDLE:
        _mdns_free_tx_packet(action->data.tx_handle.packet);
        break;
    case ACTION_RX_HANDLE:
        _mdns_packet_free(action->data.rx_handle.packet);
        break;
    case ACTION_DELEGATE_HOSTNAME_SET_ADDR:
    case ACTION_DELEGATE_HOSTNAME_ADD:
        mdns_mem_free((char *)action->data.delegate_hostname.hostname);
        free_address_list(action->data.delegate_hostname.address_list);
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
        _mdns_remap_self_service_hostname(_mdns_server->hostname, action->data.hostname_set.hostname);
        mdns_mem_free((char *)_mdns_server->hostname);
        _mdns_server->hostname = action->data.hostname_set.hostname;
        _mdns_self_host.hostname = action->data.hostname_set.hostname;
        _mdns_restart_all_pcbs();
        xSemaphoreGive(_mdns_server->action_sema);
        break;
    case ACTION_INSTANCE_SET:
        _mdns_send_bye_all_pcbs_no_instance(false);
        mdns_mem_free((char *)_mdns_server->instance);
        _mdns_server->instance = action->data.instance;
        _mdns_restart_all_pcbs_no_instance();

        break;
    case ACTION_SEARCH_ADD:
        _mdns_search_add(action->data.search_add.search);
        break;
    case ACTION_SEARCH_SEND:
        _mdns_search_send(action->data.search_add.search);
        break;
    case ACTION_SEARCH_END:
        _mdns_search_finish(action->data.search_add.search);
        break;

    case ACTION_BROWSE_ADD:
        _mdns_browse_add(action->data.browse_add.browse);
        break;
    case ACTION_BROWSE_SYNC:
        _mdns_browse_sync(action->data.browse_sync.browse_sync);
        _mdns_sync_browse_result_link_free(action->data.browse_sync.browse_sync);
        break;
    case ACTION_BROWSE_END:
        _mdns_browse_finish(action->data.browse_add.browse);
        break;

    case ACTION_TX_HANDLE: {
        mdns_tx_packet_t *p = _mdns_server->tx_queue_head;
        // packet to be handled should be at tx head, but must be consistent with the one pushed to action queue
        if (p && p == action->data.tx_handle.packet && p->queued) {
            p->queued = false; // clearing, as the packet might be reused (pushed and transmitted again)
            _mdns_server->tx_queue_head = p->next;
            _mdns_tx_handle_packet(p);
        } else {
            ESP_LOGD(TAG, "Skipping transmit of an unexpected packet!");
        }
    }
    break;
    case ACTION_RX_HANDLE:
        mdns_parse_packet(action->data.rx_handle.packet);
        _mdns_packet_free(action->data.rx_handle.packet);
        break;
    case ACTION_DELEGATE_HOSTNAME_ADD:
        if (!_mdns_delegate_hostname_add(action->data.delegate_hostname.hostname,
                                         action->data.delegate_hostname.address_list)) {
            mdns_mem_free((char *)action->data.delegate_hostname.hostname);
            free_address_list(action->data.delegate_hostname.address_list);
        }
        xSemaphoreGive(_mdns_server->action_sema);
        break;
    case ACTION_DELEGATE_HOSTNAME_SET_ADDR:
        if (!_mdns_delegate_hostname_set_address(action->data.delegate_hostname.hostname,
                                                 action->data.delegate_hostname.address_list)) {
            free_address_list(action->data.delegate_hostname.address_list);
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
 * @brief  Queue search action
 */
static esp_err_t _mdns_send_search_action(mdns_action_type_t type, mdns_search_once_t *search)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.search_add.search = search;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief  Called from timer task to run mDNS responder
 *
 * periodically checks first unqueued packet (from tx head).
 * if it is scheduled to be transmitted, then pushes the packet to action queue to be handled.
 *
 */
static void _mdns_scheduler_run(void)
{
    MDNS_SERVICE_LOCK();
    mdns_tx_packet_t *p = _mdns_server->tx_queue_head;
    mdns_action_t *action = NULL;

    // find first unqueued packet
    while (p && p->queued) {
        p = p->next;
    }
    if (!p) {
        MDNS_SERVICE_UNLOCK();
        return;
    }
    while (p && (int32_t)(p->send_at - (xTaskGetTickCount() * portTICK_PERIOD_MS)) < 0) {
        action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
        if (action) {
            action->type = ACTION_TX_HANDLE;
            action->data.tx_handle.packet = p;
            p->queued = true;
            if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
                mdns_mem_free(action);
                p->queued = false;
            }
        } else {
            HOOK_MALLOC_FAILED;
            break;
        }
        //Find the next unqued packet
        p = p->next;
    }
    MDNS_SERVICE_UNLOCK();
}

/**
 * @brief  Called from timer task to run active searches
 */
static void _mdns_search_run(void)
{
    MDNS_SERVICE_LOCK();
    mdns_search_once_t *s = _mdns_server->search_once;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (!s) {
        MDNS_SERVICE_UNLOCK();
        return;
    }
    while (s) {
        if (s->state != SEARCH_OFF) {
            if (now > (s->started_at + s->timeout)) {
                s->state = SEARCH_OFF;
                if (_mdns_send_search_action(ACTION_SEARCH_END, s) != ESP_OK) {
                    s->state = SEARCH_RUNNING;
                }
            } else if (s->state == SEARCH_INIT || (now - s->sent_at) > 1000) {
                s->state = SEARCH_RUNNING;
                s->sent_at = now;
                if (_mdns_send_search_action(ACTION_SEARCH_SEND, s) != ESP_OK) {
                    s->sent_at -= 1000;
                }
            }
        }
        s = s->next;
    }
    MDNS_SERVICE_UNLOCK();
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
    _mdns_scheduler_run();
    _mdns_search_run();
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

static esp_err_t mdns_post_custom_action_tcpip_if(mdns_if_t mdns_if, mdns_event_actions_t event_action)
{
    if (!_mdns_server || mdns_if >= MDNS_MAX_INTERFACES) {
        return ESP_ERR_INVALID_STATE;
    }

    mdns_action_t *action = (mdns_action_t *)mdns_mem_calloc(1, sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }
    action->type = ACTION_SYSTEM_EVENT;
    action->data.sys_event.event_action = event_action;
    action->data.sys_event.interface = mdns_if;

    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(action);
    }
    return ESP_OK;
}

static inline void set_default_duplicated_interfaces(void)
{
    mdns_if_t wifi_sta_if = MDNS_MAX_INTERFACES;
    mdns_if_t eth_if = MDNS_MAX_INTERFACES;
    for (mdns_if_t i = 0; i < MDNS_MAX_INTERFACES; i++) {
        if (s_esp_netifs[i].predefined && s_esp_netifs[i].predef_if == MDNS_IF_STA) {
            wifi_sta_if = i;
        }
        if (s_esp_netifs[i].predefined && s_esp_netifs[i].predef_if == MDNS_IF_ETH) {
            eth_if = i;
        }
    }
    if (wifi_sta_if != MDNS_MAX_INTERFACES && eth_if != MDNS_MAX_INTERFACES) {
        s_esp_netifs[wifi_sta_if].duplicate = eth_if;
        s_esp_netifs[eth_if].duplicate = wifi_sta_if;
    }
}

static inline void unregister_predefined_handlers(void)
{
#if MDNS_ESP_WIFI_ENABLED && (CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP)
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, mdns_preset_if_handle_system_event);
#endif
#if CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP || CONFIG_MDNS_PREDEF_NETIF_ETH
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, mdns_preset_if_handle_system_event);
#endif
#if CONFIG_ETH_ENABLED && CONFIG_MDNS_PREDEF_NETIF_ETH
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, mdns_preset_if_handle_system_event);
#endif
}

/*
 * Public Methods
 * */

esp_err_t mdns_netif_action(esp_netif_t *esp_netif, mdns_event_actions_t event_action)
{
    return mdns_post_custom_action_tcpip_if(_mdns_get_if_from_esp_netif(esp_netif), event_action);
}

esp_err_t mdns_register_netif(esp_netif_t *esp_netif)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_ERR_NO_MEM;
    MDNS_SERVICE_LOCK();
    for (mdns_if_t i = 0; i < MDNS_MAX_INTERFACES; ++i) {
        if (s_esp_netifs[i].netif == esp_netif) {
            MDNS_SERVICE_UNLOCK();
            return ESP_ERR_INVALID_STATE;
        }
    }

    for (mdns_if_t i = 0; i < MDNS_MAX_INTERFACES; ++i) {
        if (!s_esp_netifs[i].predefined && s_esp_netifs[i].netif == NULL) {
            s_esp_netifs[i].netif = esp_netif;
            err = ESP_OK;
            break;
        }
    }
    MDNS_SERVICE_UNLOCK();
    return err;
}

esp_err_t mdns_unregister_netif(esp_netif_t *esp_netif)
{
    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_ERR_NOT_FOUND;
    MDNS_SERVICE_LOCK();
    for (mdns_if_t i = 0; i < MDNS_MAX_INTERFACES; ++i) {
        if (!s_esp_netifs[i].predefined && s_esp_netifs[i].netif == esp_netif) {
            s_esp_netifs[i].netif = NULL;
            err = ESP_OK;
            break;
        }
    }
    MDNS_SERVICE_UNLOCK();
    return err;
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
    // zero-out local copy of netifs to initiate a fresh search by interface key whenever a netif ptr is needed
    for (mdns_if_t i = 0; i < MDNS_MAX_INTERFACES; ++i) {
        s_esp_netifs[i].netif = NULL;
    }

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

#if MDNS_ESP_WIFI_ENABLED && (CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP)
    if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, mdns_preset_if_handle_system_event, NULL)) != ESP_OK) {
        goto free_event_handlers;
    }
#endif
#if CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP || CONFIG_MDNS_PREDEF_NETIF_ETH
    if ((err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, mdns_preset_if_handle_system_event, NULL)) != ESP_OK) {
        goto free_event_handlers;
    }
#endif
#if CONFIG_ETH_ENABLED && CONFIG_MDNS_PREDEF_NETIF_ETH
    if ((err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, mdns_preset_if_handle_system_event, NULL)) != ESP_OK) {
        goto free_event_handlers;
    }
#endif

#if CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP || CONFIG_MDNS_PREDEF_NETIF_ETH
    set_default_duplicated_interfaces();
#endif

    uint8_t i;
#ifdef CONFIG_LWIP_IPV6
    esp_ip6_addr_t tmp_addr6;
#endif
#ifdef CONFIG_LWIP_IPV4
    esp_netif_ip_info_t if_ip_info;
#endif

    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
#ifdef CONFIG_LWIP_IPV6
        if (!esp_netif_get_ip6_linklocal(_mdns_get_esp_netif(i), &tmp_addr6) && !_ipv6_address_is_zero(tmp_addr6)) {
            _mdns_enable_pcb(i, MDNS_IP_PROTOCOL_V6);
        }
#endif
#ifdef CONFIG_LWIP_IPV4
        if (!esp_netif_get_ip_info(_mdns_get_esp_netif(i), &if_ip_info) && if_ip_info.ip.addr) {
            _mdns_enable_pcb(i, MDNS_IP_PROTOCOL_V4);
        }
#endif
    }
    if (_mdns_service_task_start()) {
        //service start failed!
        err = ESP_FAIL;
        goto free_all_and_disable_pcbs;
    }

    return ESP_OK;

free_all_and_disable_pcbs:
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        _mdns_disable_pcb(i, MDNS_IP_PROTOCOL_V6);
        _mdns_disable_pcb(i, MDNS_IP_PROTOCOL_V4);
        s_esp_netifs[i].duplicate = MDNS_MAX_INTERFACES;
    }
#if CONFIG_MDNS_PREDEF_NETIF_STA || CONFIG_MDNS_PREDEF_NETIF_AP || CONFIG_MDNS_PREDEF_NETIF_ETH
free_event_handlers:
    unregister_predefined_handlers();
#endif
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
    uint8_t i, j;
    if (!_mdns_server) {
        return;
    }

    // Unregister handlers before destroying the mdns internals to avoid receiving async events while deinit
    unregister_predefined_handlers();

    mdns_service_remove_all();
    free_delegated_hostnames();
    _mdns_service_task_stop();
    // at this point, the service task is deleted, so we can destroy the stack size
    mdns_mem_task_free(_mdns_stack_buffer);
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            mdns_pcb_deinit_local(i, j);
        }
    }
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
    while (_mdns_server->search_once) {
        mdns_search_once_t *h = _mdns_server->search_once;
        _mdns_server->search_once = h->next;
        mdns_mem_free(h->instance);
        mdns_mem_free(h->service);
        mdns_mem_free(h->proto);
        vSemaphoreDelete(h->done_semaphore);
        if (h->result) {
            _mdns_query_results_free(h->result);
        }
        mdns_mem_free(h);
    }
    while (_mdns_server->browse) {
        mdns_browse_t *b = _mdns_server->browse;
        _mdns_server->browse = _mdns_server->browse->next;
        _mdns_browse_item_free(b);

    }
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
    action->data.delegate_hostname.address_list = copy_address_list(address_list);
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
    action->data.delegate_hostname.address_list = copy_address_list(address_list);
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
    ret = _hostname_is_ours(hostname);
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

    mdns_srv_item_t *item = _mdns_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(!item, ESP_ERR_INVALID_ARG, err, TAG, "Service already exists");

    s = _mdns_create_service(service, proto, hostname, port, instance, num_items, txt);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NO_MEM, err, TAG, "Cannot create service: Out of memory");

    item = (mdns_srv_item_t *)mdns_mem_malloc(sizeof(mdns_srv_item_t));
    ESP_GOTO_ON_FALSE(item, ESP_ERR_NO_MEM, err, TAG, "Cannot create service: Out of memory");

    item->service = s;
    item->next = NULL;

    item->next = _mdns_server->services;
    _mdns_server->services = item;
    _mdns_probe_all_pcbs(&item, 1, false, false);
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
    ret = _mdns_get_service_item(service_type, proto, hostname) != NULL;
    MDNS_SERVICE_UNLOCK();
    return ret;
}

bool mdns_service_exists_with_instance(const char *instance, const char *service_type, const char *proto,
                                       const char *hostname)
{
    bool ret = false;
    MDNS_SERVICE_LOCK();
    ret = _mdns_get_service_item_instance(instance, service_type, proto, hostname) != NULL;
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
            return copy_address_list(host->address_list);
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
                    (_str_null_or_empty(instance) || _mdns_instance_name_match(srv->instance, instance))) {
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
    _mdns_query_results_free(results);
    return NULL;
}

esp_err_t mdns_service_port_set_for_host(const char *instance, const char *service, const char *proto, const char *host, uint16_t port)
{
    MDNS_SERVICE_LOCK();
    esp_err_t ret = ESP_OK;
    const char *hostname = host ? host : _mdns_server->hostname;
    ESP_GOTO_ON_FALSE(_mdns_server && _mdns_server->services && !_str_null_or_empty(service) && !_str_null_or_empty(proto) && port,
                      ESP_ERR_INVALID_ARG, err, TAG, "Invalid state or arguments");
    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance, service, proto, hostname);
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
    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance, service, proto, hostname);
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

    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance, service, proto, hostname);
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

    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance, service, proto, hostname);
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

    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance_name, service, proto, hostname);
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

    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance_name, service, proto, hostname);
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

    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance_name, service_type, proto, hostname);
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

    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance_old, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    if (s->service->instance) {
        _mdns_send_bye(&s, 1, false);
        mdns_mem_free((char *)s->service->instance);
    }
    s->service->instance = mdns_mem_strndup(instance, MDNS_NAME_BUF_LEN - 1);
    ESP_GOTO_ON_FALSE(s->service->instance, ESP_ERR_NO_MEM, err, TAG, "Out of memory");
    _mdns_probe_all_pcbs(&s, 1, false, false);

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
    mdns_srv_item_t *s = _mdns_get_service_item_instance(instance, service, proto, hostname);
    ESP_GOTO_ON_FALSE(s, ESP_ERR_NOT_FOUND, err, TAG, "Service doesn't exist");

    mdns_srv_item_t *a = _mdns_server->services;
    mdns_srv_item_t *b = a;
    if (instance) {
        while (a) {
            if (_mdns_service_match_instance(a->service, instance, service, proto, hostname)) {
                if (_mdns_server->services != a) {
                    b->next = a->next;
                } else {
                    _mdns_server->services = a->next;
                }
                _mdns_send_bye(&a, 1, false);
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
            if (_mdns_service_match(a->service, service, proto, hostname)) {
                if (_mdns_server->services != a) {
                    b->next = a->next;
                } else {
                    _mdns_server->services = a->next;
                }
                _mdns_send_bye(&a, 1, false);
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

/*
 * MDNS QUERY
 * */
void mdns_query_results_free(mdns_result_t *results)
{
    MDNS_SERVICE_LOCK();
    _mdns_query_results_free(results);
    MDNS_SERVICE_UNLOCK();
}


esp_err_t mdns_query_async_delete(mdns_search_once_t *search)
{
    if (!search) {
        return ESP_ERR_INVALID_ARG;
    }
    if (search->state != SEARCH_OFF) {
        return ESP_ERR_INVALID_STATE;
    }

    MDNS_SERVICE_LOCK();
    _mdns_search_free(search);
    MDNS_SERVICE_UNLOCK();

    return ESP_OK;
}

bool mdns_query_async_get_results(mdns_search_once_t *search, uint32_t timeout, mdns_result_t **results, uint8_t *num_results)
{
    if (xSemaphoreTake(search->done_semaphore, pdMS_TO_TICKS(timeout)) == pdTRUE) {
        if (results) {
            *results = search->result;
        }
        if (num_results) {
            *num_results = search->num_results;
        }
        return true;
    }
    return false;
}

mdns_search_once_t *mdns_query_async_new(const char *name, const char *service, const char *proto, uint16_t type,
                                         uint32_t timeout, size_t max_results, mdns_query_notify_t notifier)
{
    mdns_search_once_t *search = NULL;

    if (!_mdns_server || !timeout || _str_null_or_empty(service) != _str_null_or_empty(proto)) {
        return NULL;
    }

    search = _mdns_search_init(name, service, proto, type, type != MDNS_TYPE_PTR, timeout, max_results, notifier);
    if (!search) {
        return NULL;
    }

    if (_mdns_send_search_action(ACTION_SEARCH_ADD, search)) {
        _mdns_search_free(search);
        return NULL;
    }

    return search;
}

esp_err_t mdns_query_generic(const char *name, const char *service, const char *proto, uint16_t type, mdns_query_transmission_type_t transmission_type, uint32_t timeout, size_t max_results, mdns_result_t **results)
{
    mdns_search_once_t *search = NULL;

    *results = NULL;

    if (!_mdns_server) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!timeout || _str_null_or_empty(service) != _str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    search = _mdns_search_init(name, service, proto, type, transmission_type == MDNS_QUERY_UNICAST, timeout, max_results, NULL);
    if (!search) {
        return ESP_ERR_NO_MEM;
    }

    if (_mdns_send_search_action(ACTION_SEARCH_ADD, search)) {
        _mdns_search_free(search);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(search->done_semaphore, portMAX_DELAY);

    *results = search->result;
    _mdns_search_free(search);

    return ESP_OK;
}

esp_err_t mdns_query(const char *name, const char *service_type, const char *proto, uint16_t type, uint32_t timeout, size_t max_results, mdns_result_t **results)
{
    return mdns_query_generic(name, service_type, proto, type, type != MDNS_TYPE_PTR, timeout, max_results, results);
}

esp_err_t mdns_query_ptr(const char *service, const char *proto, uint32_t timeout, size_t max_results, mdns_result_t **results)
{
    if (_str_null_or_empty(service) || _str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    return mdns_query(NULL, service, proto, MDNS_TYPE_PTR, timeout, max_results, results);
}

esp_err_t mdns_query_srv(const char *instance, const char *service, const char *proto, uint32_t timeout, mdns_result_t **result)
{
    if (_str_null_or_empty(instance) || _str_null_or_empty(service) || _str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    return mdns_query(instance, service, proto, MDNS_TYPE_SRV, timeout, 1, result);
}

esp_err_t mdns_query_txt(const char *instance, const char *service, const char *proto, uint32_t timeout, mdns_result_t **result)
{
    if (_str_null_or_empty(instance) || _str_null_or_empty(service) || _str_null_or_empty(proto)) {
        return ESP_ERR_INVALID_ARG;
    }

    return mdns_query(instance, service, proto, MDNS_TYPE_TXT, timeout, 1, result);
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

#ifdef CONFIG_LWIP_IPV4
esp_err_t mdns_query_a(const char *name, uint32_t timeout, esp_ip4_addr_t *addr)
{
    mdns_result_t *result = NULL;
    esp_err_t err;

    if (_str_null_or_empty(name)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strstr(name, ".local")) {
        ESP_LOGW(TAG, "Please note that hostname must not contain domain name, as mDNS uses '.local' domain");
    }

    err = mdns_query(name, NULL, NULL, MDNS_TYPE_A, timeout, 1, &result);

    if (err) {
        return err;
    }

    if (!result) {
        return ESP_ERR_NOT_FOUND;
    }

    mdns_ip_addr_t *a = result->addr;
    while (a) {
        if (a->addr.type == ESP_IPADDR_TYPE_V4) {
            addr->addr = a->addr.u_addr.ip4.addr;
            mdns_query_results_free(result);
            return ESP_OK;
        }
        a = a->next;
    }

    mdns_query_results_free(result);
    return ESP_ERR_NOT_FOUND;
}
#endif /* CONFIG_LWIP_IPV4 */

#ifdef CONFIG_LWIP_IPV6
esp_err_t mdns_query_aaaa(const char *name, uint32_t timeout, esp_ip6_addr_t *addr)
{
    mdns_result_t *result = NULL;
    esp_err_t err;

    if (_str_null_or_empty(name)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strstr(name, ".local")) {
        ESP_LOGW(TAG, "Please note that hostname must not contain domain name, as mDNS uses '.local' domain");
    }

    err = mdns_query(name, NULL, NULL, MDNS_TYPE_AAAA, timeout, 1, &result);

    if (err) {
        return err;
    }

    if (!result) {
        return ESP_ERR_NOT_FOUND;
    }

    mdns_ip_addr_t *a = result->addr;
    while (a) {
        if (a->addr.type == ESP_IPADDR_TYPE_V6) {
            memcpy(addr->addr, a->addr.u_addr.ip6.addr, 16);
            mdns_query_results_free(result);
            return ESP_OK;
        }
        a = a->next;
    }

    mdns_query_results_free(result);
    return ESP_ERR_NOT_FOUND;
}
#endif /* CONFIG_LWIP_IPV6 */


/**
 * @brief  Browse sync result action
 */
esp_err_t _mdns_sync_browse_action(mdns_action_type_t type, mdns_browse_sync_t *browse_sync)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));
    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.browse_sync.browse_sync = browse_sync;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief  Browse action
 */
esp_err_t _mdns_send_browse_action(mdns_action_type_t type, mdns_browse_t *browse)
{
    mdns_action_t *action = NULL;

    action = (mdns_action_t *)mdns_mem_malloc(sizeof(mdns_action_t));

    if (!action) {
        HOOK_MALLOC_FAILED;
        return ESP_ERR_NO_MEM;
    }

    action->type = type;
    action->data.browse_add.browse = browse;
    if (xQueueSend(_mdns_server->action_queue, &action, (TickType_t)0) != pdPASS) {
        mdns_mem_free(action);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}



/**
 * @brief  Mark browse as finished, remove and free it from browse chain
 */
void _mdns_browse_finish(mdns_browse_t *browse)
{
    browse->state = BROWSE_OFF;
    mdns_browse_t *b = _mdns_server->browse;
    mdns_browse_t *target_free = NULL;
    while (b) {
        if (strlen(b->service) == strlen(browse->service) && memcmp(b->service, browse->service, strlen(b->service)) == 0 &&
                strlen(b->proto) == strlen(browse->proto) && memcmp(b->proto, browse->proto, strlen(b->proto)) == 0) {
            target_free = b;
            b = b->next;
            queueDetach(mdns_browse_t, _mdns_server->browse, target_free);
            _mdns_browse_item_free(target_free);
        } else {
            b = b->next;
        }
    }
    _mdns_browse_item_free(browse);
}

/**
 * @brief  Add new browse to the browse chain
 */
void _mdns_browse_add(mdns_browse_t *browse)
{
    browse->state = BROWSE_RUNNING;
    mdns_browse_t *queue = _mdns_server->browse;
    bool found = false;
    // looking for this browse in active browses
    while (queue) {
        if (strlen(queue->service) == strlen(browse->service) && memcmp(queue->service, browse->service, strlen(queue->service)) == 0 &&
                strlen(queue->proto) == strlen(browse->proto) && memcmp(queue->proto, browse->proto, strlen(queue->proto)) == 0) {
            found = true;
            break;
        }
        queue = queue->next;
    }
    if (!found) {
        browse->next = _mdns_server->browse;
        _mdns_server->browse = browse;
    }
    for (uint8_t interface_idx = 0; interface_idx < MDNS_MAX_INTERFACES; interface_idx++) {
        _mdns_browse_send(browse, (mdns_if_t)interface_idx);
    }
    if (found) {
        _mdns_browse_item_free(browse);
    }
}

void _mdns_browse_sync(mdns_browse_sync_t *browse_sync)
{
    mdns_browse_t *browse = browse_sync->browse;
    mdns_browse_result_sync_t *sync_result = browse_sync->sync_result;
    while (sync_result) {
        mdns_result_t *result = sync_result->result;
        DBG_BROWSE_RESULTS(result, browse_sync->browse);
        browse->notifier(result);
        if (result->ttl == 0) {
            queueDetach(mdns_result_t, browse->result, result);
            // Just free current result
            result->next = NULL;
            mdns_query_results_free(result);
        }
        sync_result = sync_result->next;
    }
}
