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

class DTE : public CommandableIf {
public:
    explicit DTE(std::unique_ptr<Terminal> t);

    ~DTE() = default;

    int write(uint8_t *data, size_t len) { return term->write(data, len); }

    int read(uint8_t **d, size_t len) {
        auto data_to_read = std::min(len, buffer_size);
        auto data = buffer.get();
        auto actual_len = term->read(data, data_to_read);
        *d = data;
        return actual_len;
    }

    void set_data_cb(std::function<bool(size_t len)> f) {
        term->set_data_cb(std::move(f));
    }

    void start() { term->start(); }

    void data_mode_closed() { term->stop(); }

    void set_mode(modem_mode m) {
        term->start();
        mode = m;
        if (m == modem_mode::DATA_MODE) {
            term->set_data_cb(on_data);
        } else if (m == modem_mode::CMUX_MODE) {
            setup_cmux();
        }
    }

    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms) override;

    void send_cmux_command(uint8_t i, const std::string &command);

private:
    Lock lock;

    void setup_cmux();

    static const size_t GOT_LINE = BIT0;
    size_t buffer_size;
    size_t consumed;
    std::unique_ptr<uint8_t[]> buffer;
    std::unique_ptr<Terminal> term;
    modem_mode mode;
    signal_group signal;
    std::function<bool(size_t len)> on_data;
};

} // namespace esp_modem

#endif // _ESP_MODEM_DTE_HPP_
