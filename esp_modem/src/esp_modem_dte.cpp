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

#include <cstring>
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"

using namespace esp_modem;

static const size_t dte_default_buffer_size = 1000;

DTE::DTE(const esp_modem_dte_config *config, std::unique_ptr<Terminal> terminal):
        buffer_size(config->dte_buffer_size), consumed(0),
        buffer(std::make_unique<uint8_t[]>(buffer_size)),
        term(std::move(terminal)), command_term(term.get()), other_term(nullptr),
        mode(modem_mode::UNDEF) {}

DTE::DTE(std::unique_ptr<Terminal> terminal):
    buffer_size(dte_default_buffer_size), consumed(0),
    buffer(std::make_unique<uint8_t[]>(buffer_size)),
    term(std::move(terminal)), command_term(term.get()), other_term(nullptr),
    mode(modem_mode::UNDEF) {}

command_result DTE::command(const std::string &command, got_line_cb got_line, uint32_t time_ms, const char separator)
{
    Scoped<Lock> l(lock);
    command_result res = command_result::TIMEOUT;
    command_term->set_read_cb([&](uint8_t *data, size_t len) {
        if (!data) {
            data = buffer.get();
            len = command_term->read(data + consumed, buffer_size - consumed);
        } else {
            consumed = 0; // if the underlying terminal contains data, we cannot fragment
        }
        if (memchr(data + consumed, separator, len)) {
            res = got_line(data, consumed + len);
            if (res == command_result::OK || res == command_result::FAIL) {
                signal.set(GOT_LINE);
                return true;
            }
        }
        consumed += len;
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

command_result DTE::command(const std::string &cmd, got_line_cb got_line, uint32_t time_ms)
{
    return command(cmd, got_line, time_ms, '\n');
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

bool DTE::set_mode(modem_mode m)
{
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

void DTE::set_read_cb(std::function<bool(uint8_t *, size_t)> f)
{
    on_data = std::move(f);
    term->set_read_cb([this](uint8_t *data, size_t len) {
        if (!data) { // if no data available from terminal callback -> need to explicitly read some
            data = buffer.get();
            len = term->read(buffer.get(), buffer_size);
        }
        if (on_data)
            return on_data(data, len);
        return false;
    });
}

int DTE::read(uint8_t **d, size_t len)
{
    auto data_to_read = std::min(len, buffer_size);
    auto data = buffer.get();
    auto actual_len = term->read(data, data_to_read);
    *d = data;
    return actual_len;
}

int DTE::write(uint8_t *data, size_t len)
{
    return term->write(data, len);
}