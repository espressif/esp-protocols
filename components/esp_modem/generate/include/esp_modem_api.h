/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
//#include "generate/esp_modem_command_declare.inc"
#include "esp_modem_c_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//  --- ESP-MODEM command module starts here ---
#include "esp_modem_command_declare_helper.inc"

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) \
        esp_err_t esp_modem_ ## name(esp_modem_dce_t *dce ESP_MODEM_COMMAND_PARAMS_AFTER(__VA_ARGS__));

#include "esp_modem_command_declare.inc"

#undef ESP_MODEM_DECLARE_DCE_COMMAND
//  --- ESP-MODEM command module ends here ---

#ifdef __cplusplus
}
#endif
