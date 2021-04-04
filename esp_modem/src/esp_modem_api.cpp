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


namespace esp_modem {

struct PdpContext;

static const char *TAG = "modem_api";

std::shared_ptr<DTE> create_uart_dte(const dte_config *config) {
    TRY_CATCH_RET_NULL(
            auto term = create_uart_terminal(config);
            return std::make_shared<DTE>(std::move(term));
    )
}

static inline std::unique_ptr<DCE>
create_modem_dce(dce_factory::Modem m, const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif) {
    dce_factory::Factory f(m);
    TRY_CATCH_RET_NULL(
            return f.build_unique(config, std::move(dte), netif);
    )
}

std::unique_ptr<DCE> create_SIM7600_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif) {
    return create_modem_dce(dce_factory::Modem::SIM7600, config, std::move(dte), netif);
}

std::unique_ptr<DCE> create_SIM800_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif) {
    return create_modem_dce(dce_factory::Modem::SIM800, config, std::move(dte), netif);
}

std::unique_ptr<DCE> create_BG96_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif) {
    return create_modem_dce(dce_factory::Modem::BG96, config, std::move(dte), netif);
}

} // namespace esp_modem

//
// C API definitions
using namespace esp_modem;

struct esp_modem_dce_wrap // need to mimic the polymorphic dispatch as CPP uses templated dispatch
{
    enum esp_modem_t { MODEM_SIM7600, MODEM_SIM800, MODEM_BG96 } modem_type;
    void * dce_ptr;
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

extern "C" esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    auto dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    if (dce_wrap == nullptr)
        return nullptr;
    auto dte = create_uart_dte(dte_config);
    auto dce = create_SIM7600_dce(dce_config, dte, netif);
    dce_wrap->modem_type = esp_modem_dce_wrap::MODEM_SIM7600;
    dce_wrap->dce_ptr = dce.release();
    return dce_wrap;
}

extern "C" void esp_modem_destroy(esp_modem_dce_t * dce)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    delete dce_sim7600;
    delete dce;
}

extern "C" esp_err_t esp_modem_set_mode(esp_modem_dce_t * dce, esp_modem_dce_mode_t mode)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    if (mode == ESP_MODEM_MODE_DATA) {
        dce_sim7600->set_data();
    } else if (mode == ESP_MODEM_MODE_COMMAND) {
        dce_sim7600->exit_data();
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_read_pin(esp_modem_dce_t * dce, bool &x)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    return command_response_to_esp_err(dce_sim7600->read_pin(x));
}

extern "C" esp_err_t esp_modem_get_signal_quality(esp_modem_dce_t * dce, int *rssi, int *ber)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    return command_response_to_esp_err(dce_sim7600->get_signal_quality(*rssi, *ber));
}

extern "C" esp_err_t esp_modem_get_imsi(esp_modem_dce_t * dce, char *p_imsi)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    std::string imsi;
    auto ret = command_response_to_esp_err(dce_sim7600->get_imsi(imsi));
    if (ret == ESP_OK && !imsi.empty()) {
        strcpy(p_imsi, imsi.c_str());
    }
    return ret;
}