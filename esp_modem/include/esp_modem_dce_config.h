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

#ifndef _ESP_MODEM_DCE_CONFIG_H_
#define _ESP_MODEM_DCE_CONFIG_H_

/** @addtogroup ESP_MODEM_CONFIG
 * @{
 */

/**
 * @brief ESP Modem DCE Default Configuration
 *
 */
#define ESP_MODEM_DCE_DEFAULT_CONFIG(APN)       \
    {                                           \
        .apn = APN                              \
    }

typedef struct esp_modem_dce_config esp_modem_dce_config_t;

/**
 * @brief DCE configuration structure
 */
struct esp_modem_dce_config {
    const char* apn;  /*!< APN: Logical name of the Access point */
};

/**
 * @}
 */


#endif // _ESP_MODEM_DCE_CONFIG_H_
