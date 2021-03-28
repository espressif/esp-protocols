//
// Created by david on 3/28/21.
//

#ifndef AP_TO_PPPOS_ESP_MODEM_DCE_CONFIG_H

struct esp_modem_dce_config {
    const char* apn;
};

#define ESP_MODEM_DCE_DEFAULT_CONFIG(APN)       \
    {                                           \
        .apn = APN                              \
    }

typedef struct esp_modem_dce_config esp_modem_dce_config_t;


#define AP_TO_PPPOS_ESP_MODEM_DCE_CONFIG_H

#endif //AP_TO_PPPOS_ESP_MODEM_DCE_CONFIG_H
