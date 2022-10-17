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

#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "cxx_include/esp_modem_primitives.hpp"
#include "cxx_include/esp_modem_terminal.hpp"
#include "cxx_include/esp_modem_cmux.hpp"
#include "cxx_include/esp_modem_types.hpp"

struct esp_modem_dte_config;

namespace esp_modem {

/**
 * @defgroup ESP_MODEM_DTE
 * @brief Definition of DTE and related classes
 */
/** @addtogroup ESP_MODEM_DTE
* @{
*/

/**
 * DTE (Data Terminal Equipment) class
 */
class DTE : public CommandableIf {
public:

    /**
     * @brief Creates a DTE instance from the terminal
     * @param config DTE config structure
     * @param t unique-ptr to Terminal
     */
    explicit DTE(const esp_modem_dte_config *config, std::unique_ptr<Terminal> t);
    explicit DTE(std::unique_ptr<Terminal> t);

    ~DTE() = default;

    /**
     * @brief Writing to the underlying terminal
     * @param data Data pointer to write
     * @param len Data len to write
     * @return number of bytes written
     */
    int write(uint8_t *data, size_t len);

    /**
     * @brief Reading from the underlying terminal
     * @param d Returning the data pointer of the received payload
     * @param len Length of the data payload
     * @return number of bytes read
     */
    int read(uint8_t **d, size_t len);

    /**
     * @brief Sets read callback with valid data and length
     * @param f Function to be called on data available
     */
    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f);

    /**
     * @brief Sets the DTE to desired mode (Command/Data/Cmux)
     * @param m Desired operation mode
     * @return true on success
     */
    [[nodiscard]] bool set_mode(modem_mode m);

    /**
     * @brief Sends command and provides callback with responding line
     * @param command String parameter representing command
     * @param got_line Function to be called after line available as a response
     * @param time_ms Time in ms to wait for the answer
     * @return OK, FAIL, TIMEOUT
     */
    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms) override;

    /**
     * @brief Sends the command (same as above) but with a specific separator
     */
    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms, char separator) override;

private:
    static const size_t GOT_LINE = SignalGroup::bit0;       /*!< Bit indicating response available */

    [[nodiscard]] bool setup_cmux();                         /*!< Internal setup of CMUX mode */

    Lock lock{};                                            /*!< Locks DTE operations */
    size_t buffer_size;                                      /*!< Size of available DTE buffer */
    size_t consumed;                                         /*!< Indication of already processed portion in DTE buffer */
    std::unique_ptr<uint8_t[]> buffer;                       /*!< DTE buffer */
    std::unique_ptr<Terminal> term;                          /*!< Primary terminal for this DTE */
    Terminal *command_term;                                  /*!< Reference to the terminal used for sending commands */
    std::unique_ptr<Terminal> other_term;                    /*!< Secondary terminal for this DTE */
    modem_mode mode;                                         /*!< DTE operation mode */
    SignalGroup signal;                                     /*!< Event group used to signal request-response operations */
    std::function<bool(uint8_t *data, size_t len)> on_data;  /*!< on data callback for current terminal */
};

/**
 * @}
 */

} // namespace esp_modem
