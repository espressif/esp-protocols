/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "sdkconfig.h"
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
 * @brief Optional overrides for console_cmd_init_with_config()
 *
 * All-zero (CONSOLE_CMD_CONFIG_DEFAULT()) reproduces console_cmd_init().
 * NULL / 0 fields keep the IDF REPL defaults. Pointers that are set must
 * remain valid until the console is deinitialized.
 *
 * @note task_core_id of 0 means "do not override". That cannot pin the REPL
 *       to CPU 0; use 1 (or another core) to pin, or -1 for no affinity.
 *       The field is ignored on ESP-IDF < 5.3.
 * @note max_cmdline_args is ignored on ESP-IDF < 6.1.
 */
typedef struct {
    const char *prompt;             /*!< NULL -> IDF default "esp> " */
    const char *history_save_path;  /*!< NULL -> history is not persisted */
    uint32_t max_history_len;       /*!< 0 -> IDF default (32) */
    uint32_t task_stack_size;       /*!< 0 -> IDF default (4096) */
    uint32_t task_priority;         /*!< 0 -> IDF default (2) */
    int task_core_id;               /*!< 0 -> do not override; -1 = no affinity */
    size_t max_cmdline_length;      /*!< 0 -> IDF default */
    size_t max_cmdline_args;        /*!< 0 -> IDF default */
} console_cmd_config_t;

#define CONSOLE_CMD_CONFIG_DEFAULT() { 0 }

/**
 * @brief Initializes the esp console
 *
 * Equivalent to console_cmd_init_with_config(NULL).
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
 * @brief Initializes the esp console with optional REPL overrides
 *
 * @param[in] config Optional overrides. NULL uses the same defaults as
 *                   console_cmd_init().
 *
 * @return
 *      - ESP_OK on success (including when already initialized)
 *      - other error codes from the underlying esp_console REPL constructor
 */
esp_err_t console_cmd_init_with_config(const console_cmd_config_t *config);

/**
 * @brief Return the current REPL handle
 *
 * @return Pointer passed to esp_console_start_repl(), or NULL if the
 *         console has not been initialized (or was stopped).
 */
esp_console_repl_t *console_cmd_get_repl(void);


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

#if CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP
/**
 * @brief Stop and delete the console REPL
 *
 * Available only when CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP is enabled.
 * On ESP-IDF < 5.5 this returns ESP_ERR_NOT_SUPPORTED.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the console is not initialized
 *      - ESP_ERR_NOT_SUPPORTED on ESP-IDF < 5.5
 *      - other error codes from esp_console_stop_repl()
 */
esp_err_t console_cmd_stop(void);
#endif

#ifdef __cplusplus
}
#endif
