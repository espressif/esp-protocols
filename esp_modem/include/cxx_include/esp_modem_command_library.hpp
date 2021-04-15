// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ESP_MODEM_COMMAND_LIBRARY_HPP_
#define _ESP_MODEM_COMMAND_LIBRARY_HPP_

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
 * @}
 */

} // dce_commands
} // esp_modem

#endif //_ESP_MODEM_COMMAND_LIBRARY_HPP_
