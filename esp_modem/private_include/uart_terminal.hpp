//
// Created by david on 2/25/21.
//

#ifndef SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP
#define SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP

#include "cxx_include/esp_modem_dte.hpp"

struct esp_modem_dte_config;

std::unique_ptr<Terminal> create_uart_terminal(const esp_modem_dte_config *config);



#endif //SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP
