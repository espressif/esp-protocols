//
// Created by david on 2/25/21.
//

#ifndef SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP
#define SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP

#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_dte_config.h"

std::unique_ptr<terminal> create_uart_terminal(const struct dte_config *config);

class dte;

std::shared_ptr<dte> create_dte(const struct dte_config *config);

std::unique_ptr<dce> create_dce(const std::shared_ptr<dte>& e, esp_netif_t *netif);


#endif //SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP
