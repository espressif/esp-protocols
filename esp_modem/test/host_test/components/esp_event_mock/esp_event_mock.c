//
// Created by david on 2/11/21.
//

#include "esp_err.h"
#include "esp_event.h"


const char * WIFI_EVENT = "WIFI_EVENT";
const char * IP_EVENT = "IP_EVENT";

esp_err_t esp_event_handler_register(const char * event_base, int32_t event_id, void* event_handler, void* event_handler_arg)
{
    return ESP_OK;
}


esp_err_t esp_event_handler_unregister(const char * event_base, int32_t event_id, void* event_handler)
{
    return ESP_OK;
}
