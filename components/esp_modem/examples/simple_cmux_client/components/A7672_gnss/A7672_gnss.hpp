/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
//
// Created on: 23.08.2022
// Author: franz


#pragma once


#include <utility>

#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

#include "a7672_common.h"
#include "a7672_gps.h"
#include "a7672_gnss.h"


/**
 * @brief Definition of a custom A7672 class with GNSS capabilities.
 * This inherits from the official esp-modem's A7672 device which contains all common library methods.
 * On top of that, the A7672_gnss adds reading GNSS information, which is implemented in a private component.
 */
class A7672_gnss: public esp_modem::A7600 {
    using A7600::A7600;
public:
    esp_modem::command_result get_gnss_information_a7672(a7672_gnss_t &gps);
    esp_modem::command_result get_gps_information_a7672(a7672_gps_t &gps);
};

namespace A7672 {
/**
 * @brief DCE for the A7672_gnss. Here we've got to forward the general commands, aa well as the GNSS one.
 */
class DCE_gnss : public esp_modem::DCE_T<A7672_gnss>, public esp_modem::CommandableIf {
public:

    using DCE_T<A7672_gnss>::DCE_T;

    esp_modem::command_result
    command(const std::string &cmd, esp_modem::got_line_cb got_line, uint32_t time_ms) override
    {
        return command(cmd, got_line, time_ms, '\n');
    }

    esp_modem::command_result
    command(const std::string &cmd, esp_modem::got_line_cb got_line, uint32_t time_ms, const char separator) override;

    int write(uint8_t *data, size_t len) override
    {
        return dte->write(data, len);
    }

    void on_read(esp_modem::got_line_cb on_data) override
    {
        return dte->on_read(on_data);
    }

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    template <typename ...Agrs> \
    esp_modem::return_type name(Agrs&&... args)   \
    {   \
        return device->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...)
    {
        device->name(...);
    } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

    esp_modem::command_result get_gnss_information_a7672(a7672_gnss_t &gps);
    esp_modem::command_result get_gps_information_a7672(a7672_gps_t &gps);

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
            return esp_modem::command_result::TIMEOUT;
        });
        handling_urc = true;
    }

private:
    esp_modem::got_line_cb handle_urc{nullptr};
    esp_modem::got_line_cb handle_cmd{nullptr};
    esp_modem::SignalGroup signal;
    bool handling_urc {false};

    esp_modem::command_result handle_data(uint8_t *data, size_t len);

};


} // namespace A7672
/**
 * @brief Helper create method which employs the customized DCE factory for building DCE_gnss objects
 * @return unique pointer of the specific DCE
 */
std::unique_ptr<A7672::DCE_gnss> create_A7672_GNSS_dce(const esp_modem::dce_config *config,
        std::shared_ptr<esp_modem::DTE> dte,
        esp_netif_t *netif);
