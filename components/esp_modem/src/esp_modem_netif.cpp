// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>
#include <esp_log.h>
#include <esp_event.h>
#include "cxx_include/esp_modem_netif.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_netif_ppp.h"


namespace esp_modem {

void Netif::on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    auto *ppp = static_cast<Netif *>(arg);
    if (event_id < NETIF_PP_PHASE_OFFSET) {
        ESP_LOGI("esp_modem_netif", "PPP state changed event %d", event_id);
        // only notify the modem on state/error events, ignoring phase transitions
        ppp->signal.set(PPP_EXIT);
    }
}

esp_err_t Netif::esp_modem_dte_transmit(void *h, void *buffer, size_t len)
{
    auto *ppp = static_cast<Netif *>(h);
    if (ppp->signal.is_any(PPP_STARTED)) {
        if (ppp->ppp_dte && ppp->ppp_dte->write((uint8_t *) buffer, len) > 0) {
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t Netif::esp_modem_post_attach(esp_netif_t *esp_netif, void *args)
{
    auto d = (ppp_netif_driver *) args;
    esp_netif_driver_ifconfig_t driver_ifconfig = {};
    driver_ifconfig.transmit = Netif::esp_modem_dte_transmit;
    driver_ifconfig.handle = (void *) d->ppp;
    d->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));
    // check if PPP error events are enabled, if not, do enable the error occurred/state changed
    // to notify the modem layer when switching modes
    esp_netif_ppp_config_t ppp_config = { .ppp_phase_event_enabled = true,    // assuming phase enabled, as earlier IDFs
                                          .ppp_error_event_enabled = false
                                        }; // don't provide cfg getters so we enable both events
#if ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 4
    esp_netif_ppp_get_params(esp_netif, &ppp_config);
#endif // ESP-IDF >= v4.4
    if (!ppp_config.ppp_error_event_enabled) {
        ppp_config.ppp_error_event_enabled = true;
        esp_netif_ppp_set_params(esp_netif, &ppp_config);
    }

    return ESP_OK;
}

void Netif::receive(uint8_t *data, size_t len)
{
    if (signal.is_any(PPP_STARTED)) {
        esp_netif_receive(driver.base.netif, data, len, nullptr);
    }
}

Netif::Netif(std::shared_ptr<DTE> e, esp_netif_t *ppp_netif) :
    ppp_dte(std::move(e)), netif(ppp_netif)
{
    driver.base.netif = ppp_netif;
    driver.ppp = this;
    driver.base.post_attach = esp_modem_post_attach;
    throw_if_esp_fail(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, (void *) this));
    throw_if_esp_fail(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, ppp_netif));
    throw_if_esp_fail(
        esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected, ppp_netif));
    throw_if_esp_fail(esp_netif_attach(ppp_netif, &driver));
}

void Netif::start()
{
    ppp_dte->set_read_cb([this](uint8_t *data, size_t len) -> bool {
        receive(data, len);
        return false;
    });
    esp_netif_action_start(driver.base.netif, nullptr, 0, nullptr);
    signal.set(PPP_STARTED);
}

void Netif::stop()
{
    esp_netif_action_stop(driver.base.netif, nullptr, 0, nullptr);
    signal.clear(PPP_STARTED);
}

Netif::~Netif()
{
    if (signal.is_any(PPP_STARTED)) {
        esp_netif_action_stop(driver.base.netif, nullptr, 0, nullptr);
        signal.clear(PPP_STARTED);
        signal.wait(PPP_EXIT, 30000);
    }
    esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected);
}

void Netif::wait_until_ppp_exits()
{
    signal.wait(PPP_EXIT, 30000);
}

} // namespace esp_modem