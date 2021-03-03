#pragma once

#include <memory>
#include "cxx_include/esp_modem_dce.hpp"
#include "cxx_include/esp_modem_device_factory.hpp"

class DTE;
struct dte_config;
typedef struct esp_netif_obj esp_netif_t;

std::shared_ptr<DTE> create_uart_dte(const dte_config *config);

std::unique_ptr<DCE> create_dce(const std::shared_ptr<DTE>& dte, const std::shared_ptr<DeviceIf>& dev, esp_netif_t *netif);

