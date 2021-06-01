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

#include <functional>
#include "cxx_include/esp_modem_types.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

namespace esp_modem {

template<typename T>
std::shared_ptr<T> create_device(const std::shared_ptr<DTE> &dte, std::string &apn)
{
    auto pdp = std::make_unique<PdpContext>(apn);
    return std::make_shared<T>(dte, std::move(pdp));
}

std::shared_ptr<GenericModule> create_generic_module(const std::shared_ptr<DTE> &dte, std::string &apn)
{
    return create_device<GenericModule>(dte, apn);
}

std::shared_ptr<SIM7600> create_SIM7600_module(const std::shared_ptr<DTE> &dte, std::string &apn)
{
    return create_device<SIM7600>(dte, apn);
}

}

#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"

namespace esp_modem::dce_factory {
std::unique_ptr<PdpContext> FactoryHelper::create_pdp_context(std::string &apn)
{
    return std::unique_ptr<PdpContext>();
}

}