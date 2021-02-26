//
// Created by david on 2/26/21.
//
#include <memory>
#include <utility>
#include <esp_log.h>
#include <esp_event.h>
#include "cxx_include/ppp_netif.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_netif_ppp.h"


//struct ppp_netif_driver {
//    esp_netif_driver_base_t base;
//};

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    dte *e = (dte*)arg;
    if (event_id < NETIF_PP_PHASE_OFFSET) {
        ESP_LOGI("TAG", "PPP state changed event %d", event_id);
        // only notify the modem on state/error events, ignoring phase transitions
        e->data_mode_closed();
//        esp_modem_notify_ppp_netif_closed(dte);
    }
}

static esp_err_t esp_modem_dte_transmit(void *h, void *buffer, size_t len)
{
    dte *e = (dte*)h;
    if (e->write((uint8_t*)buffer, len) > 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}


static esp_err_t esp_modem_post_attach(esp_netif_t * esp_netif, void * args)
{
    ppp_netif_driver *d = (ppp_netif_driver*)args;
    esp_netif_driver_ifconfig_t driver_ifconfig = { };
    driver_ifconfig.transmit = esp_modem_dte_transmit;
    driver_ifconfig.handle = (void*)d->e;

    d->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    // check if PPP error events are enabled, if not, do enable the error occurred/state changed
    // to notify the modem layer when switching modes
    esp_netif_ppp_config_t ppp_config;
    esp_netif_ppp_get_params(esp_netif, &ppp_config);
    if (!ppp_config.ppp_error_event_enabled) {
        ppp_config.ppp_error_event_enabled = true;
        esp_netif_ppp_set_params(esp_netif, &ppp_config);
    }

    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, (void*)d->e));
    return ESP_OK;
}


ppp::ppp(std::shared_ptr<dte> e, esp_netif_t *ppp_netif):
    netif(ppp_netif), _dte(std::move(e))
{
    driver.base.netif = ppp_netif;
    driver.e = this->_dte.get();
    driver.base.post_attach = esp_modem_post_attach;
    throw_if_esp_fail(esp_netif_attach(ppp_netif, &driver));
}
