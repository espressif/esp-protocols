/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <utility>
#include <cstddef>
#include <cstdint>
#include "cxx_include/esp_modem_primitives.hpp"
#include "cxx_include/esp_modem_terminal.hpp"
#include "cxx_include/esp_modem_types.hpp"
#include "cxx_include/esp_modem_buffer.hpp"

struct esp_modem_dte_config;

namespace esp_modem {

class CMux;

/**
 * @defgroup ESP_MODEM_DTE
 * @brief Definition of DTE and related classes
 */
/** @addtogroup ESP_MODEM_DTE
* @{
*/

/**
 * DTE (Data Terminal Equipment) class
 */
class DTE : public CommandableIf {
public:

    /**
     * @brief Creates a DTE instance from the terminal
     * @param config DTE config structure
     * @param t unique-ptr to Terminal
     */
    explicit DTE(const esp_modem_dte_config *config, std::unique_ptr<Terminal> t);
    explicit DTE(std::unique_ptr<Terminal> t);

    ~DTE() = default;

    /**
     * @brief Writing to the underlying terminal
     * @param data Data pointer to write
     * @param len Data len to write
     * @return number of bytes written
     */
    int write(uint8_t *data, size_t len);

    /**
     * @brief Reading from the underlying terminal
     * @param d Returning the data pointer of the received payload
     * @param len Length of the data payload
     * @return number of bytes read
     */
    int read(uint8_t **d, size_t len);

    /**
     * @brief Sets read callback with valid data and length
     * @param f Function to be called on data available
     */
    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f);

    /**
     * @brief Sets DTE error callback
     * @param f Function to be called on DTE error
     */
    void set_error_cb(std::function<void(terminal_error err)> f);

    /**
     * @brief Sets the DTE to desired mode (Command/Data/Cmux)
     * @param m Desired operation mode
     * @return true on success
     */
    [[nodiscard]] bool set_mode(modem_mode m);

    /**
     * @brief Sends command and provides callback with responding line
     * @param command String parameter representing command
     * @param got_line Function to be called after line available as a response
     * @param time_ms Time in ms to wait for the answer
     * @return OK, FAIL, TIMEOUT
     */
    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms) override;

    /**
     * @brief Sends the command (same as above) but with a specific separator
     */
    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms, char separator) override;

protected:
    /**
     * @brief Allows for locking the DTE
     */
    void lock()
    {
        internal_lock.lock();
    }
    void unlock()
    {
        internal_lock.unlock();
    }
    friend class Scoped<DTE>;                               /*!< Declaring "Scoped<DTE> lock(dte)" locks this instance */
private:
    static const size_t GOT_LINE = SignalGroup::bit0;       /*!< Bit indicating response available */

    [[nodiscard]] bool setup_cmux();                        /*!< Internal setup of CMUX mode */
    [[nodiscard]] bool exit_cmux();                         /*!< Exit of CMUX mode */

    Lock internal_lock{};                                   /*!< Locks DTE operations */
    unique_buffer buffer;                                   /*!< DTE buffer */
    std::shared_ptr<CMux> cmux_term;                        /*!< Primary terminal for this DTE */
    std::shared_ptr<Terminal> primary_term;                 /*!< Reference to the primary terminal (mostly for sending commands) */
    std::shared_ptr<Terminal> secondary_term;               /*!< Secondary terminal for this DTE */
    modem_mode mode;                                        /*!< DTE operation mode */
    SignalGroup signal;                                     /*!< Event group used to signal request-response operations */
    command_result result;                                  /*!< Command result of the currently exectuted command */
    std::function<bool(uint8_t *data, size_t len)> on_data; /*!< on data callback for current terminal */
};

/**
 * @}
 */

} // namespace esp_modem
