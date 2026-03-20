/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Modem console example: Custom DCE

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cstring>
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
//#include "generate/esp_modem_command_declare.inc"
#include "my_module_dce.hpp"

//  --- ESP-MODEM command module starts here ---
using namespace esp_modem;

//
// Define preprocessor's forwarding to dce_commands definitions
//

#define CMD_OK    (1)
#define CMD_FAIL  (2)

//
// Repeat all declarations and forward to the AT commands defined in esp_modem::dce_commands:: namespace
//
#include "esp_modem_command_declare_helper.inc"
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) \
     return_type Shiny::DCE::name(ESP_MODEM_COMMAND_PARAMS(__VA_ARGS__)) { return esp_modem::dce_commands::name(this ESP_MODEM_COMMAND_FORWARD_AFTER(__VA_ARGS__) ); }

//DECLARE_ALL_COMMAND_APIS(return_type name(...) )
#include "esp_modem_command_declare.inc"

#undef ESP_MODEM_DECLARE_DCE_COMMAND

std::unique_ptr<Shiny::DCE> create_shiny_dce(const esp_modem::dce_config *config,
                                             std::shared_ptr<esp_modem::DTE> dte,
                                             esp_netif_t *netif)
{
    return Shiny::Factory::create(config, std::move(dte), netif);
}

/**
 * @brief Definition of the command API, which makes the Shiny::DCE "command-able class"
 * @param cmd Command to send
 * @param got_line Recv line callback
 * @param time_ms timeout in ms
 * @param separator line break separator
 * @return OK, FAIL or TIMEOUT
 */
command_result Shiny::DCE::command(const std::string &cmd, got_line_cb got_line, uint32_t time_ms, const char separator)
{
    if (!handling_urc) {
        return dte->command(cmd, got_line, time_ms, separator);
    }
    handle_cmd = got_line;
    signal.clear(CMD_OK | CMD_FAIL);
    esp_modem::DTE_Command command{cmd};
    dte->write(command);
    signal.wait_any(CMD_OK | CMD_FAIL, time_ms);
    handle_cmd = nullptr;
    if (signal.is_any(CMD_OK)) {
        return esp_modem::command_result::OK;
    }
    if (signal.is_any(CMD_FAIL)) {
        return esp_modem::command_result::FAIL;
    }
    return esp_modem::command_result::TIMEOUT;
}

/**
 * @brief Handle received data
 *
 * @param data Data received from the device
 * @param len Length of the data
 * @return standard command return code (OK|FAIL|TIMEOUT)
 */
command_result Shiny::DCE::handle_data(uint8_t *data, size_t len)
{
    if (std::memchr(data, '\n', len)) {
        if (handle_urc) {
            handle_urc(data, len);
        }
        if (handle_cmd) {
            auto ret = handle_cmd(data, len);
            if (ret == esp_modem::command_result::TIMEOUT) {
                return command_result::TIMEOUT;
            }
            if (ret == esp_modem::command_result::OK) {
                signal.set(CMD_OK);
            }
            if (ret == esp_modem::command_result::FAIL) {
                signal.set(CMD_FAIL);
            }
        }
    }
    return command_result::TIMEOUT;
}
