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

//  --- ESP-MODEM command module starts here ---
#include "esp_modem_command_declare_helper.inc"
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) \
        esp_modem::return_type name(esp_modem::CommandableIf *t ESP_MODEM_COMMAND_PARAMS_AFTER(__VA_ARGS__));

#include "socket_commands.inc"
#undef ESP_MODEM_DECLARE_DCE_COMMAND

}
