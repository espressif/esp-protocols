/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_console.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the esp console
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_init(void);

/**
 * @brief Initialize Ethernet driver based on Espressif IoT Development Framework Configuration
 *
 * @param[in] cmd string that is the user defined command
 * @param[in] do_user_cmd Function pointer for a user-defined command callback function
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_user_register(char *user_cmd, esp_console_cmd_func_t do_user_cmd);

/**
 * @brief Starts the esp console
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_start(void);

#ifdef __cplusplus
}
#endif
