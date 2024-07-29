/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "console_simple_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the ping command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_ping_register(void);

/**
 * @brief Registers the getddrinfo command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_getaddrinfo_register(void);

/**
 * @brief Registers the setdnsserver command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_setdnsserver_register(void);

/**
 * @brief Registers the setdnsserver command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_getdnsserver_register(void);

#ifdef __cplusplus
}
#endif
