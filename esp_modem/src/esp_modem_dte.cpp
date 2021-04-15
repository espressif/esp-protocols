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
        term(std::move(terminal)), command_term(term.get()), other_term(nullptr),
        mode(modem_mode::UNDEF) {}

command_result DTE::command(const std::string &command, got_line_cb got_line, uint32_t time_ms, const char separator)
{
    Scoped<Lock> l(lock);
    command_result res = command_result::TIMEOUT;
    command_term->set_read_cb([&](uint8_t *data, size_t len) {
        if (!data) {
            auto data_to_read = std::min(len, buffer_size - consumed);
            data = buffer.get() + consumed;
            len = command_term->read(data, data_to_read);
        }
        consumed += len;
        if (memchr(data, separator, len)) {
            res = got_line(data, consumed);
            if (res == command_result::OK || res == command_result::FAIL) {
                signal.set(GOT_LINE);
                return true;
            }
        }
        return false;
    });
    command_term->write((uint8_t *)command.c_str(), command.length());
    auto got_lf = signal.wait(GOT_LINE, time_ms);
    if (got_lf && res == command_result::TIMEOUT) {
        throw_if_esp_fail(ESP_ERR_INVALID_STATE);
    }
    consumed = 0;
    command_term->set_read_cb(nullptr);
    return res;
}

bool DTE::setup_cmux()
{
    auto original_term = std::move(term);
    if (original_term == nullptr)
        return false;
    auto cmux_term = std::make_shared<CMux>(std::move(original_term), std::move(buffer), buffer_size);
    if (cmux_term == nullptr)
        return false;
    buffer_size = 0;
    if (!cmux_term->init())
        return false;
    term = std::make_unique<CMuxInstance>(cmux_term, 0);
    if (term == nullptr)
        return false;
    command_term = term.get(); // use command terminal as previously
    other_term = std::make_unique<CMuxInstance>(cmux_term, 1);
    return true;
}

command_result DTE::command(const std::string &cmd, got_line_cb got_line, uint32_t time_ms)
{
    return command(cmd, got_line, time_ms, '\n');
}
