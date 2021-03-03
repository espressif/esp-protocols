#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include <string.h>

#include <utility>
#include "esp_log.h"

DCE::DCE(const std::shared_ptr<DTE>& dte, std::shared_ptr<DeviceIf> device, esp_netif_t * netif):
        dce_dte(dte), device(std::move(device)), ppp_netif(dte, netif)
{ }

