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
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_terminal.hpp"
#include "vfs_termial.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include "esp_modem_config.h"
#include "exception_stub.hpp"

namespace esp_modem {

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
static const char *TAG = "modem_api_target";
#endif

std::shared_ptr<DTE> create_uart_dte(const dte_config *config)
{
    TRY_CATCH_RET_NULL(
            auto term = create_uart_terminal(config);
    return std::make_shared<DTE>(config, std::move(term));
    )
}

} // namespace esp_modem
