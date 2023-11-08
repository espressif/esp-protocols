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


/* This stucture describes the plugin to the rest of the application */
typedef struct {
    /* A pointer to the name of the command */
    const char *name;

    /* A function which performs auto-registration of console commands */
    esp_err_t (*plugin_regd_fn)(void);
} console_cmd_plugin_desc_t;


/**
 * @brief Initializes the esp console
 * @return ESP_OK on success
 */
esp_err_t console_cmd_init(void);


/**
 * @brief Register a user supplied command
 *
 * @param[in] cmd string that is the user defined command
 * @param[in] do_user_cmd Function pointer for a user-defined command callback function
 *
 * @return ESP_OK on success
 */
esp_err_t console_cmd_user_register(char *user_cmd, esp_console_cmd_func_t do_user_cmd);


/**
 * @brief Register all the console commands in .console_cmd_desc section
 *
 * @return ESP_OK on success
 */
esp_err_t console_cmd_all_register(void);


/**
 * @brief Starts the esp console
 * @return ESP_OK on success
 */
esp_err_t console_cmd_start(void);

#ifdef __cplusplus
}
#endif
