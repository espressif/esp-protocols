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

#include <thread>
#include "cxx_include/esp_modem_netif.hpp"
#include "cxx_include/esp_modem_dte.hpp"

namespace esp_modem {

void Netif::on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
}

esp_err_t Netif::esp_modem_dte_transmit(void *h, void *buffer, size_t len)
{
    auto *this_netif = static_cast<Netif *>(h);
    this_netif->ppp_dte->write((uint8_t *) buffer, len);
    return len;
}

esp_err_t Netif::esp_modem_post_attach(esp_netif_t *esp_netif, void *args)
{
    return ESP_OK;
}

void Netif::receive(uint8_t *data, size_t len)
{
    esp_netif_receive(netif, data, len);
}

Netif::Netif(std::shared_ptr<DTE> e, esp_netif_t *ppp_netif) :
    ppp_dte(std::move(e)), netif(ppp_netif) {}

void Netif::start()
{
    ppp_dte->set_read_cb([this](uint8_t *data, size_t len) -> bool {
        receive(data, len);
        return false;
    });
    netif->transmit = esp_modem_dte_transmit;
    netif->ctx = (void *)this;
    signal.set(PPP_STARTED);
}

void Netif::stop()
{
    ppp_dte->set_read_cb(nullptr);
    signal.clear(PPP_STARTED);
}

Netif::~Netif() = default;

void Netif::wait_until_ppp_exits()
{
}

} // namespace esp_modem