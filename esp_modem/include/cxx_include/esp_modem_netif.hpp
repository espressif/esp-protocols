//
// Created by david on 2/26/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_NETIF_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_NETIF_HPP

#include "esp_netif.h"
#include "cxx_include/esp_modem_primitives.hpp"

class DTE;
class Netif;

struct ppp_netif_driver {
    esp_netif_driver_base_t base;
    Netif *ppp;
};


class Netif {
public:
    explicit Netif(std::shared_ptr<DTE> e, esp_netif_t *netif);

    void start();
//    void notify_ppp_exit() { signal.set(PPP_EXIT); }
    void wait_until_ppp_exits() { signal.wait(PPP_EXIT, 50000); }
    void stop();
private:
    void receive(uint8_t *data, size_t len);
    static esp_err_t esp_modem_dte_transmit(void *h, void *buffer, size_t len);
    static esp_err_t esp_modem_post_attach(esp_netif_t * esp_netif, void * args);
    static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    std::shared_ptr<DTE> ppp_dte;
    esp_netif_t *netif;
    struct ppp_netif_driver driver{};
    signal_group signal;
    static const size_t PPP_STARTED = BIT0;
    static const size_t PPP_EXIT = BIT1;

};


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_NETIF_HPP
