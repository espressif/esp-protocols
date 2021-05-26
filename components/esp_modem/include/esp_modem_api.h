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

#pragma once

#include "esp_err.h"
#include "generate/esp_modem_command_declare.inc"
#include "esp_modem_c_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif


#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
        esp_err_t esp_modem_ ## name(esp_modem_dce_t *dce, ##__VA_ARGS__);

DECLARE_ALL_COMMAND_APIS(declares esp_modem_<API>(esp_modem_t * dce, ...);)

#undef ESP_MODEM_DECLARE_DCE_COMMAND


#ifdef __cplusplus
}
#endif
