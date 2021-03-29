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

namespace esp_modem {

enum class modem_mode {
    UNDEF,
    COMMAND_MODE,
    DATA_MODE,
    CMUX_MODE
};

enum class command_result {
    OK,
    FAIL,
    TIMEOUT
};

typedef std::function<command_result(uint8_t *data, size_t len)> got_line_cb;

struct PdpContext {
    explicit PdpContext(std::string &apn) : context_id(1), protocol_type("IP"), apn(apn) {}

    explicit PdpContext(const char *apn) : context_id(1), protocol_type("IP"), apn(apn) {}

    size_t context_id;
    std::string protocol_type;
    std::string apn;
};

class CommandableIf {
public:
    virtual command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms) = 0;
};

class ModuleIf {
public:
    virtual bool setup_data_mode() = 0;

    virtual bool set_mode(modem_mode mode) = 0;
};

} // namespace esp_modem

#endif // _ESP_MODEM_TYPES_HPP_
