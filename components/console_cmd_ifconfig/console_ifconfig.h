/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "console_simple_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the ifconfig command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_ifconfig_register(void);

#ifdef __cplusplus
}
#endif
