// Copyright 2022 Espressif Systems (Shanghai) PTE LTD
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

#pragma once

#include "cxx_include/esp_modem_dce_factory.hpp"
#include "esp_modem_c_api_types.h"

using namespace esp_modem;

struct esp_modem_dce_wrap { // need to mimic the polymorphic dispatch as CPP uses templated dispatch
    enum class modem_wrap_dte_type { UART, VFS, USB } dte_type;
    dce_factory::ModemType modem_type;
    DCE *dce;
};

inline dce_factory::ModemType convert_modem_enum(esp_modem_dce_device_t module)
{
    switch (module) {
    case ESP_MODEM_DCE_SIM7600:
        return esp_modem::dce_factory::ModemType::SIM7600;
    case ESP_MODEM_DCE_SIM7070:
        return esp_modem::dce_factory::ModemType::SIM7070;
    case ESP_MODEM_DCE_SIM7000:
        return esp_modem::dce_factory::ModemType::SIM7000;
    case ESP_MODEM_DCE_BG96:
        return esp_modem::dce_factory::ModemType::BG96;
    case ESP_MODEM_DCE_SIM800:
        return esp_modem::dce_factory::ModemType::SIM800;
    default:
    case ESP_MODEM_DCE_GENETIC:
        return esp_modem::dce_factory::ModemType::GenericModule;
    }
}
