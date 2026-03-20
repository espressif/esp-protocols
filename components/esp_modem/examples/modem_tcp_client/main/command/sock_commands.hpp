/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "cxx_include/esp_modem_types.hpp"


namespace sock_commands {

/**
 * @brief Opens network in AT command mode
 * @return OK, FAIL or TIMEOUT
 */
esp_modem::command_result net_open(esp_modem::CommandableIf *t);
/**
 * @brief Closes network in AT command mode
 * @return OK, FAIL or TIMEOUT
 */
esp_modem::command_result net_close(esp_modem::CommandableIf *t);
/**
 * @brief Opens a TCP connection
 * @param[in] host Host name or IP address to connect to
 * @param[in] port Port number
 * @param[in] timeout Connection timeout
 * @return OK, FAIL or TIMEOUT
 */
esp_modem::command_result tcp_open(esp_modem::CommandableIf *t, const std::string &host, int port, int timeout);
/**
 * @brief Closes opened TCP socket
 * @return OK, FAIL or TIMEOUT
 */
esp_modem::command_result tcp_close(esp_modem::CommandableIf *t);
/**
 * @brief Gets modem IP address
 * @param[out] addr String representation of modem's IP
 * @return OK, FAIL or TIMEOUT
 */
esp_modem::command_result get_ip(esp_modem::CommandableIf *t, std::string &addr);
/**
 * @brief Sets Rx mode
 * @param[in] mode 0=auto, 1=manual
 * @return OK, FAIL or TIMEOUT
 */
esp_modem::command_result set_rx_mode(esp_modem::CommandableIf *t, int mode);
}
