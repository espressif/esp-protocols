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
    cmux_term(nullptr), primary_term(std::move(terminal)), secondary_term(primary_term),
    mode(modem_mode::UNDEF) {}

DTE::DTE(std::unique_ptr<Terminal> terminal):
    buffer(dte_default_buffer_size),
    cmux_term(nullptr), primary_term(std::move(terminal)), secondary_term(primary_term),
    mode(modem_mode::UNDEF) {}

DTE::DTE(const esp_modem_dte_config *config, std::unique_ptr<Terminal> t, std::unique_ptr<Terminal> s):
    buffer(config->dte_buffer_size),
    cmux_term(nullptr), primary_term(std::move(t)), secondary_term(std::move(s)),
    mode(modem_mode::DUAL_MODE) {}

DTE::DTE(std::unique_ptr<Terminal> t, std::unique_ptr<Terminal> s):
    buffer(dte_default_buffer_size),
    cmux_term(nullptr), primary_term(std::move(t)), secondary_term(std::move(s)),
    mode(modem_mode::DUAL_MODE) {}

command_result DTE::command(char_span cmd, got_line_cb got_line, uint32_t time_ms, const char separator)
{
    Scoped<Lock> l(internal_lock);
    result = command_result::TIMEOUT;
    signal.clear(GOT_LINE);
    primary_term->set_read_cb([this, got_line, separator](uint8_t *data, size_t len) {
        if (!data) {
            data = buffer.get();
            len = primary_term->read(data + buffer.consumed, buffer.size - buffer.consumed);
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
    primary_term->write((uint8_t *)cmd.ptr, cmd.len);
    auto got_lf = signal.wait(GOT_LINE, time_ms);
    if (got_lf && result == command_result::TIMEOUT) {
        ESP_MODEM_THROW_IF_ERROR(ESP_ERR_INVALID_STATE);
    }
    buffer.consumed = 0;
    primary_term->set_read_cb(nullptr);
    return result;
}

bool DTE::exit_cmux()
{
    if (!cmux_term->deinit()) {
        return false;
    }
    auto ejected = cmux_term->detach();
    // return the ejected terminal and buffer back to this DTE
    primary_term = std::move(ejected.first);
    buffer = std::move(ejected.second);
    secondary_term = primary_term;
    return true;
}

bool DTE::setup_cmux()
{
    cmux_term = std::make_shared<CMux>(primary_term, std::move(buffer));
    if (cmux_term == nullptr) {
        return false;
    }
    if (!cmux_term->init()) {
        return false;
    }
    primary_term = std::make_unique<CMuxInstance>(cmux_term, 0);
    if (primary_term == nullptr) {
        return false;
    }
    secondary_term = std::make_unique<CMuxInstance>(cmux_term, 1);
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
    // transitions (COMMAND|DUAL|CMUX|UNDEF) -> DATA
    if (m == modem_mode::DATA_MODE) {
        if (mode == modem_mode::CMUX_MODE || mode == modem_mode::CMUX_MANUAL_MODE || mode == modem_mode::DUAL_MODE) {
            // mode stays the same, but need to swap terminals (as command has been switched)
            secondary_term.swap(primary_term);
        } else {
            mode = m;
        }
        return true;
    }
    // transitions (DATA|DUAL|CMUX|UNDEF) -> COMMAND
    if (m == modem_mode::COMMAND_MODE) {
        if (mode == modem_mode::CMUX_MODE) {
            if (exit_cmux()) {
                mode = m;
                return true;
            }
            mode = modem_mode::UNDEF;
            return false;
        } if (mode == modem_mode::CMUX_MANUAL_MODE || mode == modem_mode::DUAL_MODE) {
            return true;
        } else {
            mode = m;
            return true;
        }
    }
    // manual CMUX transitions: Enter CMUX
    if (m == modem_mode::CMUX_MANUAL_MODE) {
        if (setup_cmux()) {
            mode = m;
            return true;
        }
        mode = modem_mode::UNDEF;
        return false;
    }
    // manual CMUX transitions: Exit CMUX
    if (m == modem_mode::CMUX_MANUAL_EXIT && mode == modem_mode::CMUX_MANUAL_MODE) {
        if (exit_cmux()) {
            mode = modem_mode::COMMAND_MODE;
            return true;
        }
        mode = modem_mode::UNDEF;
        return false;
    }
    // manual CMUX transitions: Swap terminals
    if (m == modem_mode::CMUX_MANUAL_SWAP && mode == modem_mode::CMUX_MANUAL_MODE) {
        secondary_term.swap(primary_term);
        return true;
    }
    mode = modem_mode::UNDEF;
    return false;
}

void DTE::set_read_cb(std::function<bool(uint8_t *, size_t)> f)
{
    on_data = std::move(f);
    secondary_term->set_read_cb([this](uint8_t *data, size_t len) {
        if (!data) { // if no data available from terminal callback -> need to explicitly read some
            data = buffer.get();
            len = secondary_term->read(buffer.get(), buffer.size);
        }
        if (on_data) {
            return on_data(data, len);
        }
        return false;
    });
}

void DTE::set_error_cb(std::function<void(terminal_error err)> f)
{
    secondary_term->set_error_cb(f);
    primary_term->set_error_cb(f);
}

int DTE::read(uint8_t **d, size_t len)
{
    auto data_to_read = std::min(len, buffer.size);
    auto data = buffer.get();
    auto actual_len = secondary_term->read(data, data_to_read);
    *d = data;
    return actual_len;
}

int DTE::write(uint8_t *data, size_t len)
{
    return secondary_term->write(data, len);
}

int DTE::write(DTE_Command command)
{
    return primary_term->write(command.data, command.len);
}

void DTE::on_read(got_line_cb on_read_cb)
{
    if (on_read_cb == nullptr) {
        primary_term->set_read_cb(nullptr);
        internal_lock.unlock();
        return;
    }
    internal_lock.lock();
    primary_term->set_read_cb([this, on_read_cb](uint8_t *data, size_t len) {
        if (!data) {
            data = buffer.get();
            len = primary_term->read(data, buffer.size);
        }
        auto res = on_read_cb(data, len);
        if (res == command_result::OK || res == command_result::FAIL) {
            primary_term->set_read_cb(nullptr);
            internal_lock.unlock();
            return true;
        }
        return false;
    });
}

/**
 * Implemented here to keep all headers C++11 compliant
 */
unique_buffer::unique_buffer(size_t size):
    data(std::make_unique<uint8_t[]>(size)), size(size), consumed(0) {}
