//
// Created by david on 2/26/21.
//

#ifndef SIMPLE_CXX_CLIENT_PPP_NETIF_HPP
#define SIMPLE_CXX_CLIENT_PPP_NETIF_HPP

#include "esp_netif.h"
#include "cxx_include/terminal_objects.hpp"

class DTE;

//struct ppp_netif_driver;
struct ppp_netif_driver {
    esp_netif_driver_base_t base;
    DTE *e;
};


class ppp {
public:
    explicit ppp(std::shared_ptr<DTE> e, esp_netif_t *netif);

    void start();
    void notify_ppp_exit() { signal.set(PPP_EXIT); }
    void wait_until_ppp_exits() { signal.wait(PPP_EXIT, 50000); }
    void stop();
private:
    void receive(uint8_t *data, size_t len) const;
    std::shared_ptr<DTE> ppp_dte;
    esp_netif_t *netif;
    struct ppp_netif_driver driver;
    signal_group signal;
    const size_t PPP_EXIT = BIT0;

};


#endif //SIMPLE_CXX_CLIENT_PPP_NETIF_HPP
