//
// Created by david on 2/25/21.
//

#ifndef SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP
#define SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP

#include "cxx_include/esp_modem_dte.hpp"


struct dte_config;

std::unique_ptr<terminal> create_uart_terminal(const dte_config *config);



#endif //SIMPLE_CXX_CLIENT_UART_TERMINAL_HPP
