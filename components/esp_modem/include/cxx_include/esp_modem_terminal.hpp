/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <functional>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "esp_err.h"
#include "esp_modem_primitives.hpp"

namespace esp_modem {

/**
 * @defgroup ESP_MODEM_TERMINAL
 * @brief Definition of an abstract terminal to be attached to DTE class
 */

/** @addtogroup ESP_MODEM_TERMINAL
* @{
*/

/**
 * @brief Terminal errors
 */
enum class terminal_error {
    BUFFER_OVERFLOW,
    CHECKSUM_ERROR,
    UNEXPECTED_CONTROL_FLOW,
    DEVICE_GONE,
};

/**
 * @brief Terminal interface. All communication interfaces must comply to this interface in order to be used as a DTE
 */
class Terminal {
public:
    virtual ~Terminal() = default;

    void set_error_cb(std::function<void(terminal_error)> f)
    {
        Scoped<Lock> l(cb_lock);
        on_error = std::move(f);
    }

    virtual void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f)
    {
        Scoped<Lock> l(cb_lock);
        on_read = std::move(f);
    }

    /**
     * @brief Writes data to the terminal
     * @param data Data pointer
     * @param len Data len
     * @return length of data written
     */
    virtual int write(uint8_t *data, size_t len) = 0;

    /**
     * @brief Read from the terminal. This function doesn't block, but return all available data.
     * @param data Data pointer to store the read payload
     * @param len Maximum data len to read
     * @return length of data actually read
     */
    virtual int read(uint8_t *data, size_t len) = 0;

    virtual void start() = 0;

    virtual void stop() = 0;

protected:
    /**
     * @brief Serializes (re)assignment of on_read/on_error (set_read_cb/set_error_cb) against their
     * invocation by the terminal's RX task. Recursive, so a callback may re-enter set_read_cb()
     * (e.g. DTE clears its own read callback from within the callback). Holding this lock while
     * clearing a callback guarantees no callback is in flight afterwards, which is what makes it
     * safe to tear down the state the callback captures.
     */
    Lock cb_lock{};
    std::function<bool(uint8_t *data, size_t len)> on_read;
    std::function<void(terminal_error)> on_error;
};

/**
 * @}
 */

} // namespace esp_modem
