#pragma once

#include <memory>
#include "cxx_include/esp_modem_dce.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

class DTE;
class GenericModule;
class SIM7600;
class SIM800;
class BG96;
struct dte_config;
typedef struct esp_netif_obj esp_netif_t;

std::shared_ptr<DTE> create_uart_dte(const dte_config *config);


std::shared_ptr<GenericModule> create_generic_module(const std::shared_ptr<DTE>& dte, std::string &apn);
std::shared_ptr<SIM7600> create_SIM7600_module(const std::shared_ptr<DTE>& dte, std::string &apn);

std::unique_ptr<DCE<GenericModule>> create_generic_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<GenericModule>& dev, esp_netif_t *netif);
std::unique_ptr<DCE<SIM7600>> create_SIM7600_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<SIM7600>& dev, esp_netif_t *netif);


std::unique_ptr<DCE<SIM7600>> create_SIM7600_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn);
