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

#ifndef _ESP_MODEM_C_API_TYPES_H_
#define _ESP_MODEM_C_API_TYPES_H_

#include "esp_modem_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_modem_dce_wrap esp_modem_dce_t;

struct PdpContext;
typedef enum esp_modem_dce_mode
{
    ESP_MODEM_MODE_COMMAND,
    ESP_MODEM_MODE_DATA,
} esp_modem_dce_mode_t;

esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif);

void esp_modem_destroy(esp_modem_dce_t * dce);
esp_err_t esp_modem_set_mode(esp_modem_dce_t * dce, esp_modem_dce_mode_t mode);


#ifdef __cplusplus
}
#endif

#endif //_ESP_MODEM_C_API_TYPES_H_
