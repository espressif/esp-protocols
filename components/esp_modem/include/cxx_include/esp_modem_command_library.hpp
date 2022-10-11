/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_modem_dte.hpp"
#include "esp_modem_dce_module.hpp"
#include "esp_modem_types.hpp"
#include "generate/esp_modem_command_declare.inc"

namespace esp_modem {
namespace dce_commands {

/**
 * @defgroup ESP_MODEM_DCE_COMMAND ESP_MODEM DCE command library
 * @brief Library of the most useful DCE commands
 */
/** @addtogroup ESP_MODEM_DCE_COMMAND
 * @{
 */

/**
 * @brief Declaration of all commands is generated from esp_modem_command_declare.inc
 */
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
        return_type name(CommandableIf *t, ## __VA_ARGS__);

DECLARE_ALL_COMMAND_APIS(declare name(Commandable *p, ...);)

#undef ESP_MODEM_DECLARE_DCE_COMMAND

/**
 * @brief Following commands that are different for some specific modules
 */
command_result get_battery_status_sim7xxx(CommandableIf *t, int &voltage, int &bcs, int &bcl);
command_result set_gnss_power_mode_sim76xx(CommandableIf *t, int mode);
command_result power_down_sim76xx(CommandableIf *t);
command_result power_down_sim70xx(CommandableIf *t);
command_result set_network_bands_sim76xx(CommandableIf *t, const std::string &mode, const int *bands, int size);
command_result power_down_sim8xx(CommandableIf *t);
command_result set_data_mode_sim8xx(CommandableIf *t);

/**
 * @}
 */

} // dce_commands
} // esp_modem
