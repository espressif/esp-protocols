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

#ifndef _ESP_MODEM_DTE_HPP_
#define _ESP_MODEM_DTE_HPP_

#include <memory>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "cxx_include/esp_modem_primitives.hpp"
#include "cxx_include/esp_modem_terminal.hpp"
#include "cxx_include/esp_modem_cmux.hpp"
#include "cxx_include/esp_modem_types.hpp"

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
    explicit DTE(std::unique_ptr<Terminal> t);

    ~DTE() = default;

    /**
     * @brief Writing to the underlying terminal
     * @param data Data pointer to write
     * @param len Data len to write
     * @return number of bytes written
     */
    int write(uint8_t *data, size_t len) { return term->write(data, len); }

    /**
     * @brief Reading from the underlying terminal
     * @param d Returning the data pointer of the received payload
     * @param len Length of the data payload
     * @return number of bytes read
     */
    int read(uint8_t **d, size_t len) {
        auto data_to_read = std::min(len, buffer_size);
        auto data = buffer.get();
        auto actual_len = term->read(data, data_to_read);
        *d = data;
        return actual_len;
    }

    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f)
    {
        on_data = std::move(f);
        term->set_read_cb([this](uint8_t *data, size_t len) {
            if (!data) {
                auto data_to_read = std::min(len, buffer_size - consumed);
                data = buffer.get();
                len = term->read(data, data_to_read);
            }
            if (on_data)
                return on_data(data, len);
            return false;
        });
    }

    void start() { term->start(); }

    [[nodiscard]] bool set_mode(modem_mode m) {
        term->start();
        mode = m;
        if (m == modem_mode::DATA_MODE) {
            term->set_read_cb(on_data);
            if (other_term) { // if we have the other terminal, let's use it for commands
                command_term = other_term.get();
            }
        } else if (m == modem_mode::CMUX_MODE) {
            return setup_cmux();
        }
        return true;
    }

    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms) override;
    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms, const char separator) override;


private:
    Lock lock;

    [[nodiscard]] bool setup_cmux();

    static const size_t GOT_LINE = signal_group::bit0;
    size_t buffer_size;
    size_t consumed;
    std::unique_ptr<uint8_t[]> buffer;
    std::unique_ptr<Terminal> term;
    Terminal *command_term;
    std::unique_ptr<Terminal> other_term;
    modem_mode mode;
    signal_group signal;
    std::function<bool(uint8_t *data, size_t len)> on_data;
};

/**
 * @}
 */

} // namespace esp_modem

#endif // _ESP_MODEM_DTE_HPP_
