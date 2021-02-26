//
// Created by david on 2/26/21.
//

#ifndef SIMPLE_CXX_CLIENT_PPP_NETIF_HPP
#define SIMPLE_CXX_CLIENT_PPP_NETIF_HPP

#include "esp_netif.h"

class dte;

//struct ppp_netif_driver;
struct ppp_netif_driver {
    esp_netif_driver_base_t base;
    dte *e;
};


class ppp {
public:
    explicit ppp(std::shared_ptr<dte> e, esp_netif_t *netif);

private:
    esp_netif_t *netif;
    std::shared_ptr<dte> _dte;
    struct ppp_netif_driver driver;
};


#endif //SIMPLE_CXX_CLIENT_PPP_NETIF_HPP
