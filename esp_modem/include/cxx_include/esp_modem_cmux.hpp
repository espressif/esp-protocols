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

#ifndef _ESP_MODEM_CMUX_HPP_
#define _ESP_MODEM_CMUX_HPP_

namespace esp_modem {
/**
 * @defgroup ESP_MODEM_CMUX ESP_MODEM CMUX class
 * @brief Definition of CMUX terminal
 */
/** @addtogroup ESP_MODEM_CMUX
* @{
*/

/**
 * @brief CMUX state machine
 */
enum class cmux_state {
    INIT,
    HEADER,
    PAYLOAD,
    FOOTER,
    RECOVER,
};

/**
 * @brief CMUX terminal abstraction
 *
 * This class inherits from Terminal class, as it is a Terminal, but is also composed of another Terminal,
 * which is used to communicate with the modem, i.e. the original Terminal which has been multiplexed.
 *
 * @note Implementation of CMUX protocol is experimental
 */
class CMUXedTerminal: public Terminal {
public:
    explicit CMUXedTerminal(std::unique_ptr<Terminal> t, std::unique_ptr<uint8_t[]> b, size_t buff_size):
            term(std::move(t)), buffer(std::move(b)) {}
    ~CMUXedTerminal() override = default;
    void setup_cmux();
    void set_data_cb(std::function<bool(size_t len)> f) override {}
    int write(uint8_t *data, size_t len) override;
    int read(uint8_t *data, size_t len)  override { return term->read(data, len); }
    void start() override;
    void stop() override {}
private:
    static bool process_cmux_recv(size_t len);
    void send_sabm(size_t i);
    std::unique_ptr<Terminal> term;
    cmux_state state;
    uint8_t dlci;
    uint8_t type;
    size_t payload_len;
    uint8_t frame_header[6];
    size_t frame_header_offset;
    size_t buffer_size;
    size_t consumed;
    std::unique_ptr<uint8_t[]> buffer;
    bool on_cmux(size_t len);
};

/**
 * @}
 */

} // namespace esp_modem

#endif // _ESP_MODEM_CMUX_HPP_
