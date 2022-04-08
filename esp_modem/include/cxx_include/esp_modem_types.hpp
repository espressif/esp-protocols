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

#ifndef _ESP_MODEM_TYPES_HPP_
#define _ESP_MODEM_TYPES_HPP_

#include <functional>
#include <string>
#include <cstddef>
#include <cstdint>

namespace esp_modem {

/**
 * @defgroup ESP_MODEM_TYPES
 * @brief Basic type definitions used in esp-modem
 */

/** @addtogroup ESP_MODEM_TYPES
* @{
*/

/**
 * @brief Modem working mode
 */
enum class modem_mode {
    UNDEF,
    COMMAND_MODE, /*!< Command mode -- the modem is supposed to send AT commands in this mode  */
    DATA_MODE,    /*!< Data mode -- the modem communicates with network interface on PPP protocol */
    CMUX_MODE     /*!< CMUX (Multiplex mode) -- Simplified CMUX mode, which creates two virtual terminals,
                   *  assigning one solely to command interface and the other  to the data mode */
};

/**
 * @brief Module command result
 */
enum class command_result {
    OK,             /*!< The command completed successfully */
    FAIL,           /*!< The command explicitly failed */
    TIMEOUT         /*!< The device didn't respond in the specified timeline */
};

typedef std::function<command_result(uint8_t *data, size_t len)> got_line_cb;

/**
 * @brief PDP context used for configuring and setting the data mode up
 */
struct PdpContext {
    explicit PdpContext(std::string apn) : apn(std::move(apn)) {}
    size_t context_id = 1;
    std::string protocol_type = "IP";
    std::string apn;
};

/**
 * @brief Interface for classes eligible to send AT commands (Modules, DCEs, DTEs)
 */
class CommandableIf {
public:
    /**
     * @brief Sends custom AT command
     * @param command Command to be sent
     * @param got_line callback if a line received
     * @param time_ms timeout in milliseconds
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms) = 0;
};

/**
 * @brief Interface for classes implementing a module for the modem
 */
class ModuleIf {
public:
    /**
     * @brief Sets the data mode up (provides the necessary configuration to connect to the cellular network
     * @return true on success
     */
    virtual bool setup_data_mode() = 0;

    /**
     * @brief Sets the operation mode
     * @param mode Desired mode
     * @return true on success
     */
    virtual bool set_mode(modem_mode mode) = 0;
};

/**
 * @}
 */

} // namespace esp_modem

#endif // _ESP_MODEM_TYPES_HPP_
