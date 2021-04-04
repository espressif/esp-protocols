//
// Created by david on 2/9/21.
//

#ifndef MDNS_HOST_ESP_EVENT_BASE_H
#define MDNS_HOST_ESP_EVENT_BASE_H

typedef enum {
    WIFI_EVENT_STA_CONNECTED,            /**< ESP32 station connected to AP */
    WIFI_EVENT_STA_DISCONNECTED,         /**< ESP32 station disconnected from AP */
    WIFI_EVENT_AP_START,                 /**< ESP32 soft-AP start */
    WIFI_EVENT_AP_STOP,                  /**< ESP32 soft-AP stop */
    IP_EVENT_STA_GOT_IP,
    IP_EVENT_GOT_IP6
} mdns_used_event_t;

#endif //MDNS_HOST_ESP_EVENT_BASE_H
