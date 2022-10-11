/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_cmux.hpp"
#include "esp_modem_config.h"

using namespace esp_modem;

static const size_t dte_default_buffer_size = 1000;

DTE::DTE(const esp_modem_dte_config *config, std::unique_ptr<Terminal> terminal):
    buffer(config->dte_buffer_size),
    cmux_term(nullptr), command_term(std::move(terminal)), data_term(command_term),
    mode(modem_mode::UNDEF) {}

DTE::DTE(std::unique_ptr<Terminal> terminal):
    buffer(dte_default_buffer_size),
    cmux_term(nullptr), command_term(std::move(terminal)), data_term(command_term),
    mode(modem_mode::UNDEF) {}

command_result DTE::command(const std::string &command, got_line_cb got_line, uint32_t time_ms, const char separator)
{
    Scoped<Lock> l(internal_lock);
    result = command_result::TIMEOUT;
    signal.clear(GOT_LINE);
    command_term->set_read_cb([this, got_line, separator](uint8_t *data, size_t len) {
        if (!data) {
            data = buffer.get();
            len = command_term->read(data + buffer.consumed, buffer.size - buffer.consumed);
        } else {
            buffer.consumed = 0; // if the underlying terminal contains data, we cannot fragment
        }
        if (memchr(data + buffer.consumed, separator, len)) {
            result = got_line(data, buffer.consumed + len);
            if (result == command_result::OK || result == command_result::FAIL) {
                signal.set(GOT_LINE);
                return true;
            }
        }
        buffer.consumed += len;
        return false;
    });
    command_term->write((uint8_t *)command.c_str(), command.length());
    auto got_lf = signal.wait(GOT_LINE, time_ms);
    if (got_lf && result == command_result::TIMEOUT) {
        ESP_MODEM_THROW_IF_ERROR(ESP_ERR_INVALID_STATE);
    }
    buffer.consumed = 0;
    command_term->set_read_cb(nullptr);
    return result;
}

command_result DTE::command(const std::string &cmd, got_line_cb got_line, uint32_t time_ms)
{
    return command(cmd, got_line, time_ms, '\n');
}

bool DTE::exit_cmux()
{
    if (!cmux_term->deinit()) {
        return false;
    }
    auto ejected = cmux_term->detach();
    // return the ejected terminal and buffer back to this DTE
    command_term = std::move(ejected.first);
    buffer = std::move(ejected.second);
    data_term = command_term;
    return true;
}

bool DTE::setup_cmux()
{
    cmux_term = std::make_shared<CMux>(command_term, std::move(buffer));
    if (cmux_term == nullptr) {
        return false;
    }
    if (!cmux_term->init()) {
        return false;
    }
    command_term = std::make_unique<CMuxInstance>(cmux_term, 0);
    if (command_term == nullptr) {
        return false;
    }
    data_term = std::make_unique<CMuxInstance>(cmux_term, 1);
    return true;
}

bool DTE::set_mode(modem_mode m)
{
    // transitions (COMMAND|UNDEF) -> CMUX
    if (m == modem_mode::CMUX_MODE) {
        if (mode == modem_mode::UNDEF || mode == modem_mode::COMMAND_MODE) {
            if (setup_cmux()) {
                mode = m;
                return true;
            }
            mode = modem_mode::UNDEF;
            return false;
        }
    }
    // transitions (COMMAND|CMUX|UNDEF) -> DATA
    if (m == modem_mode::DATA_MODE) {
        if (mode == modem_mode::CMUX_MODE) {
            // mode stays the same, but need to swap terminals (as command has been switch)
            data_term.swap(command_term);
        } else {
            mode = m;
        }
        // prepare the data terminal's callback to the configured std::function (used by netif)
        data_term->set_read_cb(on_data);
        return true;
    }
    // transitions (DATA|CMUX|UNDEF) -> COMMAND
    if (m == modem_mode::COMMAND_MODE) {
        if (mode == modem_mode::CMUX_MODE) {
            if (exit_cmux()) {
                mode = m;
                return true;
            }
            mode = modem_mode::UNDEF;
            return false;
        } else {
            mode = m;
            return true;
        }
    }
    return true;
}

void DTE::set_read_cb(std::function<bool(uint8_t *, size_t)> f)
{
    on_data = std::move(f);
    data_term->set_read_cb([this](uint8_t *data, size_t len) {
        if (!data) { // if no data available from terminal callback -> need to explicitly read some
            data = buffer.get();
            len = data_term->read(buffer.get(), buffer.size);
        }
        if (on_data) {
            return on_data(data, len);
        }
        return false;
    });
}

void DTE::set_error_cb(std::function<void(terminal_error err)> f)
{
    data_term->set_error_cb(f);
    command_term->set_error_cb(f);
}

int DTE::read(uint8_t **d, size_t len)
{
    auto data_to_read = std::min(len, buffer.size);
    auto data = buffer.get();
    auto actual_len = data_term->read(data, data_to_read);
    *d = data;
    return actual_len;
}

int DTE::write(uint8_t *data, size_t len)
{
    return data_term->write(data, len);
}

/**
 * Implemented here to keep all headers C++11 compliant
 */
unique_buffer::unique_buffer(size_t size):
    data(std::make_unique<uint8_t[]>(size)), size(size), consumed(0) {}
