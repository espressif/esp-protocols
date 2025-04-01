/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "mdns_private.h"
#include "mdns_networking.h"
#include "mdns_pcb.h"
#include "mdns_netif.h"
#include "mdns_utils.h"
#include "mdns_send.h"
#include "mdns_mem_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "mdns_responder.h"

static const char *TAG = "mdns_responder";

typedef struct {
    mdns_pcb_state_t state;
    mdns_srv_item_t **probe_services;
    uint8_t probe_services_len;
    uint8_t probe_ip;
    uint8_t probe_running;
    uint16_t failed_probes;
} mdns_pcb_t;

static mdns_pcb_t s_pcbs[MDNS_MAX_INTERFACES][MDNS_IP_PROTOCOL_MAX];

/**
 * @brief  Send announcement on particular PCB
 */
void mdns_priv_pcb_announce(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool include_ip)
{
    mdns_pcb_t *_pcb = &s_pcbs[tcpip_if][ip_protocol];
    size_t i;
    if (mdns_priv_if_ready(tcpip_if, ip_protocol)) {
        if (PCB_STATE_IS_PROBING(_pcb)) {
            mdns_priv_init_pcb_probe(tcpip_if, ip_protocol, services, len, include_ip);
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

            if (mdns_utils_str_null_or_empty(mdns_priv_get_global_hostname())) {
                return;
            }

            _pcb->state = PCB_ANNOUNCE_1;
            mdns_tx_packet_t *p = _mdns_create_announce_packet(tcpip_if, ip_protocol, services, len, include_ip);
            if (p) {
                mdns_send_schedule_tx_packet(p, 0);
            }
        }
    }
}

/**
 * @brief  Check if interface is duplicate (two interfaces on the same subnet)
 */
bool mdns_priv_pcb_check_for_duplicates(mdns_if_t tcpip_if)
{
    mdns_if_t ifaces[MDNS_MAX_INTERFACES] = {tcpip_if, mdns_priv_netif_get_other_interface(tcpip_if) };
    if (ifaces[1] == MDNS_MAX_INTERFACES) {
        return false;
    }
    // check both this netif and the potential duplicate on all protocols
    // if any of them is in duplicate state, return true
    for (int i = 0; i < 2; i++) {
        for (int proto = 0; proto < MDNS_IP_PROTOCOL_MAX; proto++) {
            if (s_pcbs[ifaces[i]][(mdns_ip_protocol_t) proto].state == PCB_DUP) {
                return true;
            }
        }
    }
    return false;
}

static esp_err_t mdns_pcb_deinit_local(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_proto)
{
    esp_err_t err = mdns_priv_if_deinit(tcpip_if, ip_proto);
    mdns_pcb_t *_pcb = &s_pcbs[tcpip_if][ip_proto];
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
 * @brief  Restart the responder on particular PCB
 */
static void _mdns_restart_pcb(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    size_t srv_count = 0;
    mdns_srv_item_t *a = mdns_priv_get_services();
    while (a) {
        srv_count++;
        a = a->next;
    }
    if (srv_count == 0) {
        // proble only IP
        mdns_priv_init_pcb_probe(tcpip_if, ip_protocol, NULL, 0, true);
        return;
    }
    mdns_srv_item_t *services[srv_count];
    size_t i = 0;
    a = mdns_priv_get_services();
    while (a) {
        services[i++] = a;
        a = a->next;
    }
    mdns_priv_init_pcb_probe(tcpip_if, ip_protocol, services, srv_count, true);
}

/**
 * @brief  Disable mDNS interface
 */
void mdns_priv_pcb_disable(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    mdns_priv_netif_disable(tcpip_if);

    if (mdns_priv_if_ready(tcpip_if, ip_protocol)) {
        _mdns_clear_pcb_tx_queue_head(tcpip_if, ip_protocol);
        mdns_pcb_deinit_local(tcpip_if, ip_protocol);
        mdns_if_t other_if = mdns_priv_netif_get_other_interface(tcpip_if);
        if (other_if != MDNS_MAX_INTERFACES && s_pcbs[other_if][ip_protocol].state == PCB_DUP) {
            s_pcbs[other_if][ip_protocol].state = PCB_OFF;
            mdns_priv_pcb_enable(other_if, ip_protocol);
        }
    }
    s_pcbs[tcpip_if][ip_protocol].state = PCB_OFF;
}

/**
 * @brief  Enable mDNS interface
 */
void mdns_priv_pcb_enable(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    if (!mdns_priv_if_ready(tcpip_if, ip_protocol)) {
        if (mdns_priv_if_init(tcpip_if, ip_protocol)) {
            s_pcbs[tcpip_if][ip_protocol].failed_probes = 0;
            return;
        }
    }
    _mdns_restart_pcb(tcpip_if, ip_protocol);
}

/**
 * @brief  Set interface as duplicate if another is found on the same subnet
 */
void mdns_priv_pcb_set_duplicate(mdns_if_t tcpip_if)
{
    uint8_t i;
    mdns_if_t other_if = mdns_priv_netif_get_other_interface(tcpip_if);
    if (other_if == MDNS_MAX_INTERFACES) {
        return; // no other interface found
    }
    for (i = 0; i < MDNS_IP_PROTOCOL_MAX; i++) {
        if (mdns_priv_if_ready(other_if, i)) {
            //stop this interface and mark as dup
            if (mdns_priv_if_ready(tcpip_if, i)) {
                _mdns_clear_pcb_tx_queue_head(tcpip_if, i);
                mdns_pcb_deinit_local(tcpip_if, i);
            }
            s_pcbs[tcpip_if][i].state = PCB_DUP;
//            _mdns_server->interfaces[tcpip_if].pcbs[i].state = PCB_DUP;
            mdns_priv_pcb_announce(other_if, i, NULL, 0, true);
        }
    }
}

bool mdns_priv_pcb_is_off(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    return s_pcbs[tcpip_if][ip_protocol].state == PCB_OFF;
}

void mdns_priv_pcb_schedule_tx_packet(mdns_tx_packet_t *p)
{
    mdns_pcb_t *pcb = &s_pcbs[p->tcpip_if][p->ip_protocol];
    mdns_out_question_t *q = NULL;
    mdns_tx_packet_t *a = NULL;
    uint32_t send_after = 1000;
    switch (pcb->state) {
    case PCB_PROBE_1:
        q = p->questions;
        while (q) {
            q->unicast = false;
            q = q->next;
        }
    //fallthrough
    case PCB_PROBE_2:
        mdns_send_schedule_tx_packet(p, 250);
        pcb->state = (mdns_pcb_state_t)((uint8_t)(pcb->state) + 1);
        break;
    case PCB_PROBE_3:
        a = _mdns_create_announce_from_probe(p);
        if (!a) {
            mdns_send_schedule_tx_packet(p, 250);
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
        mdns_send_schedule_tx_packet(p, send_after);
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

void mdns_priv_pcb_check_probing_services(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_service_t *service, bool removed_answers, bool *should_remove_questions)
{
    mdns_pcb_t *_pcb = &s_pcbs[tcpip_if][ip_protocol];

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
            *should_remove_questions = true;
            return;
        }
    } else if (PCB_STATE_IS_ANNOUNCING(_pcb)) {
        //if answers were cleared, set to running
        if (removed_answers) {
            _pcb->state = PCB_RUNNING;
        }
    }
    *should_remove_questions = false;
}

void mdns_priv_pcb_deinit(void)
{
    for (int i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (int j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            mdns_pcb_deinit_local(i, j);
        }
    }
}

bool mdsn_priv_pcb_is_inited(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    return mdns_priv_if_ready(tcpip_if, ip_protocol) && s_pcbs[tcpip_if][ip_protocol].state > PCB_INIT;
}

bool mdns_priv_pcb_is_duplicate(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol)
{
    return s_pcbs[tcpip_if][ip_protocol].state == PCB_DUP;
}

bool mdns_priv_pcb_is_probing(mdns_rx_packet_t *packet)
{
    return s_pcbs[packet->tcpip_if][packet->ip_protocol].probe_running;
}

bool mdns_priv_pcb_is_after_probing(mdns_rx_packet_t *packet)
{
    return s_pcbs[packet->tcpip_if][packet->ip_protocol].state > PCB_PROBE_3;
}

void mdns_priv_pcb_set_probe_failed(mdns_rx_packet_t *packet)
{
    s_pcbs[packet->tcpip_if][packet->ip_protocol].failed_probes++;
}




/**
 * @brief  Send probe for additional services on particular PCB
 */
static void _mdns_init_pcb_probe_new_service(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool probe_ip)
{
    mdns_pcb_t *pcb = &s_pcbs[tcpip_if][ip_protocol];
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
    mdns_send_schedule_tx_packet(packet, ((pcb->failed_probes > 5) ? 1000 : 120) + (esp_random() & 0x7F));
    pcb->state = PCB_PROBE_1;
}

void mdns_priv_init_pcb_probe(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, mdns_srv_item_t **services, size_t len, bool probe_ip)
{
    mdns_pcb_t *pcb = &s_pcbs[tcpip_if][ip_protocol];

    _mdns_clear_pcb_tx_queue_head(tcpip_if, ip_protocol);

    if (mdns_utils_str_null_or_empty(mdns_priv_get_global_hostname())) {
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
 * @brief  Send by for particular services
 */
void mdns_priv_pcb_send_bye_service(mdns_srv_item_t **services, size_t len, bool include_ip)
{
    uint8_t i, j;
    if (mdns_utils_str_null_or_empty(mdns_priv_get_global_hostname())) {
        return;
    }

    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            if (mdns_priv_if_ready(i, j) && s_pcbs[i][j].state == PCB_RUNNING) {
                mdns_send_bye_pcb((mdns_if_t) i, (mdns_ip_protocol_t) j, services, len, include_ip);
            }
        }
    }
}

void mdns_priv_probe_all_pcbs(mdns_srv_item_t **services, size_t len, bool probe_ip, bool clear_old_probe)
{
    uint8_t i, j;
    for (i = 0; i < MDNS_MAX_INTERFACES; i++) {
        for (j = 0; j < MDNS_IP_PROTOCOL_MAX; j++) {
            if (mdns_priv_if_ready(i, j)) {
                mdns_pcb_t *_pcb = &s_pcbs[i][j];
                if (clear_old_probe) {
                    mdns_mem_free(_pcb->probe_services);
                    _pcb->probe_services = NULL;
                    _pcb->probe_services_len = 0;
                    _pcb->probe_running = false;
                }
                mdns_priv_init_pcb_probe((mdns_if_t) i, (mdns_ip_protocol_t) j, services, len, probe_ip);
            }
        }
    }
}
