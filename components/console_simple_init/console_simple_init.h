/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_console.h"

#ifdef __cplusplus
extern "C" {
#endif


/* This structure describes the plugin to the rest of the application */
typedef struct {
    /* A pointer to the name of the command */
    const char *name;

    /* A function which performs auto-registration of console commands */
    esp_err_t (*plugin_regd_fn)(void);
} console_cmd_plugin_desc_t;


/**
 * @brief Initializes the esp console
 *
 * Safe to call more than once. A second successful call is a no-op and
 * returns ESP_OK so existing ESP_ERROR_CHECK() callers keep working.
 *
 * @return
 *      - ESP_OK on success (including when already initialized)
 *      - other error codes from the underlying esp_console REPL constructor
 */
esp_err_t console_cmd_init(void);


/**
 * @brief Register a user supplied command
 *
 * @param[in] user_cmd string that is the user defined command. Must remain
 *                     valid until the console is deinitialized.
 * @param[in] do_user_cmd Function pointer for a user-defined command callback function
 *
 * @return
 *      - ESP_OK on success
 *      - other error codes from esp_console_cmd_register()
 */
esp_err_t console_cmd_user_register(const char *user_cmd, esp_console_cmd_func_t do_user_cmd);


/**
 * @brief Register all the console commands in .console_cmd_desc section
 *
 * On failure the name of the plugin that failed is logged.
 *
 * @return
 *      - ESP_OK on success
 *      - error code from the first plugin registration function that fails
 */
esp_err_t console_cmd_all_register(void);


/**
 * @brief Starts the esp console
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if console_cmd_init() has not succeeded
 *      - other error codes from esp_console_start_repl()
 */
esp_err_t console_cmd_start(void);

#ifdef __cplusplus
}
#endif
