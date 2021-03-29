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

#ifndef _ESP_MODEM_NETIF_HPP
#define _ESP_MODEM_NETIF_HPP

#include "esp_netif.h"
#include "cxx_include/esp_modem_primitives.hpp"

namespace esp_modem {

class DTE;
class Netif;

struct ppp_netif_driver {
    esp_netif_driver_base_t base;
    Netif *ppp;
};

class Netif {
public:
    explicit Netif(std::shared_ptr<DTE> e, esp_netif_t *netif);

    ~Netif();

    void start();

    void wait_until_ppp_exits() { signal.wait(PPP_EXIT, 30000); }

    void stop();

private:
    void receive(uint8_t *data, size_t len);

    static esp_err_t esp_modem_dte_transmit(void *h, void *buffer, size_t len);

    static esp_err_t esp_modem_post_attach(esp_netif_t *esp_netif, void *args);

    static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    std::shared_ptr<DTE> ppp_dte;
    esp_netif_t *netif;
    struct ppp_netif_driver driver{};
    signal_group signal;
    static const size_t PPP_STARTED = BIT0;
    static const size_t PPP_EXIT = BIT1;
};

} // namespace esp_modem

#endif // _ESP_MODEM_NETIF_HPP
