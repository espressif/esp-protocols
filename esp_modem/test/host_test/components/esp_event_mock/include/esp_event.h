//
// Created by david on 2/9/21.
//

#ifndef MDNS_HOST_ESP_EVENT_H
#define MDNS_HOST_ESP_EVENT_H

#include <stdint.h>
#include "esp_netif_ip_addr.h"
#include "esp_err.h"


typedef void * esp_event_base_t;
typedef void * system_event_t;

extern const char * WIFI_EVENT;
extern const char * IP_EVENT;

#define ESP_EVENT_ANY_BASE     NULL             /**< register handler for any event base */
#define ESP_EVENT_ANY_ID       -1               /**< register handler for any event id */


typedef struct {
    int if_index;                    /*!< Interface index for which the event is received (left for legacy compilation) */
    esp_netif_t *esp_netif;          /*!< Pointer to corresponding esp-netif object */
    esp_netif_ip6_info_t ip6_info;   /*!< IPv6 address of the interface */
    int ip_index;                    /*!< IPv6 address index */
} ip_event_got_ip6_t;


esp_err_t esp_event_handler_register(const char * event_base, int32_t event_id, void* event_handler, void* event_handler_arg);

esp_err_t esp_event_handler_unregister(const char * event_base, int32_t event_id, void* event_handler);

#endif //MDNS_HOST_ESP_EVENT_H
