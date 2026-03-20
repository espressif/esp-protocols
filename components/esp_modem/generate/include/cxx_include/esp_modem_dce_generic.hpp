/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

//  --- ESP-MODEM command module starts here ---
namespace esp_modem {

/**
 * @defgroup ESP_MODEM_DCE
 * @brief Definition of DCE abstraction
 */

/** @addtogroup ESP_MODEM_DCE
 * @{
 */

/**
 * @brief Common abstraction of the modem DCE, specialized by the GenericModule which is a parent class for the supported
 * devices and most common modems, as well.
 */
class DCE : public DCE_T<GenericModule> {
public:

    command_result get_operator_name(std::string &name)
    {
        return device->get_operator_name(name);
    }

    using DCE_T<GenericModule>::DCE_T;
#include "esp_modem_command_declare_helper.inc"
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) \
    return_type name(ESP_MODEM_COMMAND_PARAMS(__VA_ARGS__))   \
    { \
        return device->name(ESP_MODEM_COMMAND_FORWARD(__VA_ARGS__)); \
    }
#include "esp_modem_command_declare.inc"

#undef ESP_MODEM_DECLARE_DCE_COMMAND

};

/**
 * @}
 */

} // esp_modem
