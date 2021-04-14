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

#include "esp_modem_terminal.hpp"

namespace esp_modem {

constexpr size_t max_terms = 2;
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
class CMuxInstance;

/**
 * @brief CMux class which consumes the original terminal and creates multiple virtual terminals from it.
 * This class is not usable applicable as a DTE terminal
 */
class CMux {
public:
    explicit CMux(std::unique_ptr<Terminal> t, std::unique_ptr<uint8_t[]> b, size_t buff_size):
            term(std::move(t)), buffer_size(buff_size), buffer(std::move(b)) {}
    ~CMux() = default;
    void init();
    void set_read_cb(int inst, std::function<bool(uint8_t *data, size_t len)> f);

    int write(int i, uint8_t *data, size_t len);
private:
    std::function<bool(uint8_t *data, size_t len)> read_cb[max_terms];
    void data_available(uint8_t *data, size_t len);
    void send_sabm(size_t i);
    std::unique_ptr<Terminal> term;
    cmux_state state;
    uint8_t dlci;
    uint8_t type;
    size_t payload_len;
    uint8_t frame_header[6];
    size_t frame_header_offset;
    size_t buffer_size;
    std::unique_ptr<uint8_t[]> buffer;
    bool on_cmux(uint8_t *data, size_t len);
    static uint8_t fcs_crc(const uint8_t frame[6]);
    Lock lock;
    int instance;
};

/**
 * @brief This represents a specific instance of a CMUX virtual terminal. This class also implements Terminal interface
 * and as such could be used as a DTE's terminal.
 */
class CMuxInstance: public Terminal {
public:
    explicit CMuxInstance(std::shared_ptr<CMux> parent, int i): cmux(std::move(parent)), instance(i) {}

    int write(uint8_t *data, size_t len) override { return cmux->write(instance, data, len); }
    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f) override { return cmux->set_read_cb(instance, std::move(f)); }
    int read(uint8_t *data, size_t len) override { return  0; }
    void start() override { }
    void stop() override { }
private:
    std::shared_ptr<CMux> cmux;
    int instance;
};

/**
 * @}
 */

} // namespace esp_modem

#endif // _ESP_MODEM_CMUX_HPP_
