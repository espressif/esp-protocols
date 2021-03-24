//
// Created by david on 2/26/21.
//
#include <memory>
#include <utility>
#include <esp_log.h>
#include <esp_event.h>
#include "cxx_include/esp_modem_netif.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_netif_ppp.h"

#include <iostream>


void PPP::on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    PPP *ppp = (PPP*)arg;
//    DTE *e = (DTE*)arg;
    std::cout << "on_ppp_changed " << std::endl;
    ESP_LOGW("TAG", "PPP state changed event %d", event_id);
    if (event_id < NETIF_PP_PHASE_OFFSET) {
        ESP_LOGI("TAG", "PPP state changed event %d", event_id);
        // only notify the modem on state/error events, ignoring phase transitions
        ppp->signal.set(PPP_EXIT);
//        e->notify_ppp_exit();
//        e->data_mode_closed();
//        esp_modem_notify_ppp_netif_closed(dte);
    }
}

esp_err_t PPP::esp_modem_dte_transmit(void *h, void *buffer, size_t len)
{
    PPP *ppp = (PPP*)h;
    if (ppp->signal.is_any(PPP_STARTED)) {
        std::cout << "sending data " << len << std::endl;
        if (ppp->ppp_dte->write((uint8_t*)buffer, len) > 0) {
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t PPP::esp_modem_post_attach(esp_netif_t * esp_netif, void * args)
{
    auto d = (ppp_netif_driver*)args;
    esp_netif_driver_ifconfig_t driver_ifconfig = { };
    driver_ifconfig.transmit = PPP::esp_modem_dte_transmit;
    driver_ifconfig.handle = (void*)d->ppp;
    std::cout << "esp_modem_post_attach " << std::endl;
    d->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    // check if PPP error events are enabled, if not, do enable the error occurred/state changed
    // to notify the modem layer when switching modes
    esp_netif_ppp_config_t ppp_config;
//    esp_netif_ppp_get_params(esp_netif, &ppp_config);
    if (!ppp_config.ppp_error_event_enabled) {
        ppp_config.ppp_error_event_enabled = true;
        esp_netif_ppp_set_params(esp_netif, &ppp_config);
    }

//    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, 0));
    return ESP_OK;
}

void PPP::receive(uint8_t *data, size_t len)
{
    if (signal.is_any(PPP_STARTED)) {
        std::cout << "received data " << len << std::endl;
        esp_netif_receive(driver.base.netif, data, len, nullptr);
    }
}

PPP::PPP(std::shared_ptr<DTE> e, esp_netif_t *ppp_netif):
    ppp_dte(std::move(e)), netif(ppp_netif)
{
    driver.base.netif = ppp_netif;
    driver.ppp = this;
    driver.base.post_attach = esp_modem_post_attach;
    throw_if_esp_fail(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, (void*)this));
    throw_if_esp_fail(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, ppp_netif));
    throw_if_esp_fail(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected, ppp_netif));
    throw_if_esp_fail(esp_netif_attach(ppp_netif, &driver));
}

void PPP::start()
{
    ppp_dte->set_data_cb([this](size_t len) -> bool {
        uint8_t *data;
        auto actual_len = ppp_dte->read(&data, len);
        receive(data, actual_len);
        return false;
    });
    esp_netif_action_start(driver.base.netif, 0, 0, 0);
    signal.set(PPP_STARTED);
}

void PPP::stop()
{
    std::cout << "esp_netif_action_stop "  << std::endl;
    esp_netif_action_stop(driver.base.netif, nullptr, 0, nullptr);
    signal.clear(PPP_STARTED);

}