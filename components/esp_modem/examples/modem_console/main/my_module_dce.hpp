/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * Modem console example: Custom DCE
*/

#pragma once


#include <utility>

#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

/**
 * @brief Definition of a custom modem which inherits from the GenericModule, uses all its methods
 * and could override any of them. Here, for demonstration purposes only, we redefine just `get_module_name()`
 */
class MyShinyModem: public esp_modem::GenericModule {
    using GenericModule::GenericModule;
public:
    esp_modem::command_result get_module_name(std::string &name) override
    {
        name = "Custom Shiny Module";
        return esp_modem::command_result::OK;
    }
};

namespace Shiny {

using namespace esp_modem;

class DCE : public esp_modem::DCE_T<MyShinyModem>, public CommandableIf {
public:
    using DCE_T<MyShinyModem>::DCE_T;

    command_result
    command(const std::string &cmd, got_line_cb got_line, uint32_t time_ms, const char separator = '\n') override;

    int write(uint8_t *data, size_t len) override
    {
        return dte->write(data, len);
    }

    void on_read(got_line_cb on_data) override
    {
        return dte->on_read(on_data);
    }

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    esp_modem::return_type name(__VA_ARGS__);

    DECLARE_ALL_COMMAND_APIS(forwards name(...))

#undef ESP_MODEM_DECLARE_DCE_COMMAND

    void set_on_read(esp_modem::got_line_cb on_read_cb)
    {
        if (on_read_cb == nullptr) {
            handling_urc = false;
            handle_urc = nullptr;
            dte->on_read(nullptr);
            return;
        }
        handle_urc = std::move(on_read_cb);
        dte->on_read([this](uint8_t *data, size_t len) {
            this->handle_data(data, len);
            return command_result::TIMEOUT;
        });
        handling_urc = true;
    }

private:
    got_line_cb handle_urc{nullptr};
    got_line_cb handle_cmd{nullptr};
    SignalGroup signal;
    bool handling_urc {false};

    command_result handle_data(uint8_t *data, size_t len);

};

class Factory: public ::esp_modem::dce_factory::Factory {
public:

    static std::unique_ptr<DCE> create(const esp_modem::dce_config *config,
                                       std::shared_ptr<esp_modem::DTE> dte,
                                       esp_netif_t *netif)
    {
        return build_generic_DCE<MyShinyModem, DCE, std::unique_ptr<DCE>>(config, std::move(dte), netif);
    }

};

} // namespace Shiny

/**
 * @brief Helper create method which employs the DCE factory for creating DCE objects templated by a custom module
 * @return unique pointer of the resultant DCE
 */
std::unique_ptr<Shiny::DCE> create_shiny_dce(const esp_modem::dce_config *config,
        std::shared_ptr<esp_modem::DTE> dte,
        esp_netif_t *netif);
