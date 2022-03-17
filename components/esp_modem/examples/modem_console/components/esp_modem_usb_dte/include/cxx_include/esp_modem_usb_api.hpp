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

#pragma once
#include "cxx_include/esp_modem_dce.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

namespace esp_modem {
/**
 * @brief Create USB DTE
 * 
 * @param config DTE configuration
 * @return shared ptr to DTE on success
 *         nullptr on failure (either due to insufficient memory or wrong dte configuration)
 *         if exceptions are disabled the API abort()'s on error 
 */
std::shared_ptr<DTE> create_usb_dte(const dte_config *config);
}
