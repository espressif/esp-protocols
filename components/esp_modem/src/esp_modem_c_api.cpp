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

#include <cassert>
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_terminal.hpp"
#include "esp_log.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include "esp_modem_c_api_types.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "cstring"

//
// C API definitions
using namespace esp_modem;

struct esp_modem_dce_wrap // need to mimic the polymorphic dispatch as CPP uses templated dispatch
{
    enum class modem_wrap_dte_type { UART, } dte_type;
    dce_factory::Modem modem_type;
    DCE* dce;
};

static inline esp_err_t command_response_to_esp_err(command_result res)
{
    switch (res) {
        case command_result::OK:
            return ESP_OK;
        case command_result::FAIL:
            return ESP_FAIL;
        case command_result::TIMEOUT:
            return ESP_ERR_TIMEOUT;
    }
    return ESP_ERR_INVALID_ARG;
}

static inline dce_factory::Modem convert_modem_enum(esp_modem_dce_device_t module)
{
    switch (module) {
        case ESP_MODEM_DCE_SIM7600:
            return esp_modem::dce_factory::Modem::SIM7600;
        case ESP_MODEM_DCE_BG96:
            return esp_modem::dce_factory::Modem::BG96;
        case ESP_MODEM_DCE_SIM800:
            return esp_modem::dce_factory::Modem::SIM800;
        default:
        case ESP_MODEM_DCE_GENETIC:
            return esp_modem::dce_factory::Modem::GenericModule;
    }
}

extern "C" esp_modem_dce_t *esp_modem_new_dev(esp_modem_dce_device_t module, const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    auto dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    if (dce_wrap == nullptr)
        return nullptr;
    auto dte = create_uart_dte(dte_config);
    if (dte == nullptr) {
        delete dce_wrap;
        return nullptr;
    }
    dce_factory::Factory f(convert_modem_enum(module));
    dce_wrap->dce = f.build(dce_config, std::move(dte), netif);
    if (dce_wrap->dce == nullptr) {
        delete dce_wrap;
        return nullptr;
    }
    dce_wrap->modem_type = convert_modem_enum(module);
    dce_wrap->dte_type = esp_modem_dce_wrap::modem_wrap_dte_type::UART;
    return dce_wrap;
}

extern "C" esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    return esp_modem_new_dev(ESP_MODEM_DCE_GENETIC, dte_config, dce_config, netif);
}

extern "C" void esp_modem_destroy(esp_modem_dce_t * dce_wrap)
{
    if (dce_wrap) {
        delete dce_wrap->dce;
        delete dce_wrap;
    }
}

extern "C" esp_err_t esp_modem_set_mode(esp_modem_dce_t * dce_wrap, esp_modem_dce_mode_t mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;
    if (mode == ESP_MODEM_MODE_DATA) {
        dce_wrap->dce->set_data();
    } else if (mode == ESP_MODEM_MODE_COMMAND) {
        dce_wrap->dce->exit_data();
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_read_pin(esp_modem_dce_t * dce_wrap, bool *pin)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;

    return command_response_to_esp_err(dce_wrap->dce->read_pin(*pin));
}

extern "C" esp_err_t esp_modem_sms_txt_mode(esp_modem_dce_t * dce_wrap, bool txt)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;

    return command_response_to_esp_err(dce_wrap->dce->sms_txt_mode(txt));
}

extern "C" esp_err_t esp_modem_send_sms(esp_modem_dce_t * dce_wrap, const char * number, const char * message)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;
    std::string number_str(number);
    std::string message_str(message);
    return command_response_to_esp_err(dce_wrap->dce->send_sms(number_str, message_str));
}

extern "C" esp_err_t esp_modem_sms_character_set(esp_modem_dce_t * dce_wrap)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;

    return command_response_to_esp_err(dce_wrap->dce->sms_character_set());
}

extern "C" esp_err_t esp_modem_set_pin(esp_modem_dce_t * dce_wrap, const char *pin)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;
    std::string pin_str(pin);
    return command_response_to_esp_err(dce_wrap->dce->set_pin(pin_str));
}

extern "C" esp_err_t esp_modem_get_signal_quality(esp_modem_dce_t * dce_wrap, int *rssi, int *ber)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;
    return command_response_to_esp_err(dce_wrap->dce->get_signal_quality(*rssi, *ber));
}

extern "C" esp_err_t esp_modem_get_imsi(esp_modem_dce_t * dce_wrap, char *p_imsi)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr)
        return ESP_ERR_INVALID_ARG;
    std::string imsi;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_imsi(imsi));
    if (ret == ESP_OK && !imsi.empty()) {
        strcpy(p_imsi, imsi.c_str());
    }
    return ret;
}