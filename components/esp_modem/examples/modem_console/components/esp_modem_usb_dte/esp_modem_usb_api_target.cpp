// Copyright 2021-2022 Espressif Systems (Shanghai) PTE LTD
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


#include "usb_terminal.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_netif.hpp"

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
static const char *TAG = "modem_usb_api_target";
#endif

namespace esp_modem {
std::shared_ptr<DTE> create_usb_dte(const dte_config *config)
{
    TRY_CATCH_RET_NULL(
        auto term = create_usb_terminal(config);
        return std::make_shared<DTE>(config, std::move(term));
    )
}
}
