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

#include "cxx_include/esp_modem_dte.hpp"
#include <cstring>
#include "esp_log.h"

using namespace esp_modem;

const int DTE_BUFFER_SIZE = 1024;

DTE::DTE(std::unique_ptr<Terminal> terminal):
        buffer_size(DTE_BUFFER_SIZE), consumed(0),
        buffer(std::make_unique<uint8_t[]>(buffer_size)),
        term(std::move(terminal)), mode(modem_mode::UNDEF) {}

command_result DTE::command(const std::string &command, got_line_cb got_line, uint32_t time_ms)
{
    Scoped<Lock> l(lock);
    command_result res = command_result::TIMEOUT;
    term->write((uint8_t *)command.c_str(), command.length());
    term->set_data_cb([&](size_t len){
        auto data_to_read = std::min(len, buffer_size - consumed);
        auto data = buffer.get() + consumed;
        auto actual_len = term->read(data, data_to_read);
        consumed += actual_len;
        if (memchr(data, '\n', actual_len)) {
            res = got_line(buffer.get(), consumed);
            if (res == command_result::OK || res == command_result::FAIL) {
                signal.set(GOT_LINE);
                return true;
            }
        }
        return false;
    });
    auto got_lf = signal.wait(GOT_LINE, time_ms);
    if (got_lf && res == command_result::TIMEOUT) {
        throw_if_esp_fail(ESP_ERR_INVALID_STATE);
    }
    consumed = 0;
    term->set_data_cb(nullptr);
    return res;
}