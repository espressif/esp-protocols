//
// Created by david on 3/8/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_API_H
#define SIMPLE_CXX_CLIENT_ESP_MODEM_API_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_modem_dce_wrap esp_modem_t;

esp_modem_t *esp_modem_new(const esp_modem_dte_config *config, esp_netif_t *netif, const char* apn);

void esp_modem_destroy(esp_modem_t * dce);


#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
        esp_err_t esp_modem_ ## name(esp_modem_t *dce, ##__VA_ARGS__);

DECLARE_ALL_COMMAND_APIS(declares esp_modem_<API>(esp_modem_t * dce, ...);)

#undef ESP_MODEM_DECLARE_DCE_COMMAND


#ifdef __cplusplus
}
#endif


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_API_H
