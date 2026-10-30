/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "console_simple_init.h"
#if CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP
#include "console_simple_init_priv.h"
#endif


static esp_console_repl_t *s_repl = NULL;
static const char *TAG = "console_simple_init";

static void apply_repl_config(esp_console_repl_config_t *repl_config, const console_cmd_config_t *config)
{
    if (config == NULL) {
        return;
    }

    if (config->prompt != NULL) {
        repl_config->prompt = config->prompt;
    }
    if (config->history_save_path != NULL) {
        repl_config->history_save_path = config->history_save_path;
    }
    if (config->max_history_len != 0) {
        repl_config->max_history_len = config->max_history_len;
    }
    if (config->task_stack_size != 0) {
        repl_config->task_stack_size = config->task_stack_size;
    }
    if (config->task_priority != 0) {
        repl_config->task_priority = config->task_priority;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    if (config->task_core_id != 0) {
        repl_config->task_core_id = config->task_core_id;
    }
#endif
    if (config->max_cmdline_length != 0) {
        repl_config->max_cmdline_length = config->max_cmdline_length;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 1, 0)
    if (config->max_cmdline_args != 0) {
        repl_config->max_cmdline_args = config->max_cmdline_args;
    }
#endif
}

static esp_err_t create_repl(const esp_console_repl_config_t *repl_config)
{
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    return esp_console_new_repl_uart(&hw_config, repl_config, &s_repl);

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_cdc(&hw_config, repl_config, &s_repl);

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_serial_jtag(&hw_config, repl_config, &s_repl);

#else
#error Unsupported console type
#endif
}

esp_err_t console_cmd_init(void)
{
    return console_cmd_init_with_config(NULL);
}

esp_err_t console_cmd_init_with_config(const console_cmd_config_t *config)
{
    if (s_repl != NULL) {
        ESP_LOGW(TAG, "console already initialized");
        return ESP_OK;
    }

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    apply_repl_config(&repl_config, config);
    return create_repl(&repl_config);
}

esp_console_repl_t *console_cmd_get_repl(void)
{
    return s_repl;
}

#if CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP
void console_cmd_internal_clear_repl(void)
{
    s_repl = NULL;
}
#endif

static esp_err_t register_cmd(const esp_console_cmd_t *cmd)
{
    if (s_repl == NULL) {
        ESP_LOGE(TAG, "console_cmd_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_console_cmd_register(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to register cmd '%s': %s",
                 (cmd && cmd->command) ? cmd->command : "(null)",
                 esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t console_cmd_user_register(const char *user_cmd, esp_console_cmd_func_t do_user_cmd)
{
    const esp_console_cmd_t cmd = {
        .command = user_cmd,
        .help = "User defined command",
        .hint = NULL,
        .func = do_user_cmd,
    };
    return register_cmd(&cmd);
}

esp_err_t console_cmd_register(const char *cmd, const char *help, const char *hint,
                               esp_console_cmd_func_t func)
{
    const esp_console_cmd_t desc = {
        .command = cmd,
        .help = help,
        .hint = hint,
        .func = func,
    };
    return register_cmd(&desc);
}

esp_err_t console_cmd_register_with_args(const char *cmd, const char *help, void *argtable,
                                         esp_console_cmd_func_t func)
{
    const esp_console_cmd_t desc = {
        .command = cmd,
        .help = help,
        .hint = NULL,
        .func = func,
        .argtable = argtable,
    };
    return register_cmd(&desc);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
esp_err_t console_cmd_register_with_context(const char *cmd, const char *help, const char *hint,
                                            esp_console_cmd_func_with_context_t func, void *context)
{
    const esp_console_cmd_t desc = {
        .command = cmd,
        .help = help,
        .hint = hint,
        .func = NULL,
        .func_w_context = func,
        .context = context,
    };
    return register_cmd(&desc);
}
#endif

esp_err_t console_cmd_unregister(const char *cmd)
{
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0)
    (void)cmd;
    ESP_LOGE(TAG, "console_cmd_unregister() requires ESP-IDF >= 5.4");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_repl == NULL) {
        ESP_LOGE(TAG, "console_cmd_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_console_cmd_deregister(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to unregister cmd '%s': %s",
                 cmd ? cmd : "(null)", esp_err_to_name(ret));
    }
    return ret;
#endif
}

esp_err_t console_cmd_run(const char *cmdline, int *cmd_ret)
{
    if (s_repl == NULL) {
        ESP_LOGE(TAG, "console_cmd_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_console_run(cmdline, cmd_ret);
    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Command not found: '%s'", cmdline ? cmdline : "(null)");
    } else if (ret == ESP_OK && cmd_ret != NULL && *cmd_ret != 0) {
        ESP_LOGW(TAG, "Command '%s' returned %d", cmdline, *cmd_ret);
    }
    return ret;
}

/* Linker symbols from SURROUND(console_cmd_array). Declared as arrays so
 * walking the section is not an out-of-bounds access of a single object. */
static const console_cmd_plugin_desc_t *plugin_array_begin(void)
{
    extern const console_cmd_plugin_desc_t _console_cmd_array_start[];
    return (const console_cmd_plugin_desc_t *)(uintptr_t)_console_cmd_array_start;
}

static const console_cmd_plugin_desc_t *plugin_array_end(void)
{
    extern const console_cmd_plugin_desc_t _console_cmd_array_end[];
    return (const console_cmd_plugin_desc_t *)(uintptr_t)_console_cmd_array_end;
}

esp_err_t console_cmd_all_register(void)
{
    ESP_LOGI(TAG, "List of Console commands:");
    for (const console_cmd_plugin_desc_t *it = plugin_array_begin(); it != plugin_array_end(); ++it) {
        ESP_LOGI(TAG, "- Command '%s', function plugin_regd_fn=%p", it->name, it->plugin_regd_fn);
        if (it->plugin_regd_fn == NULL) {
            continue;
        }
        esp_err_t ret = (it->plugin_regd_fn)();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register plugin '%s': %s", it->name, esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGD(TAG, "Registered plugin '%s' (fn=%p)", it->name, it->plugin_regd_fn);
    }

    return ESP_OK;
}

size_t console_cmd_plugin_count(void)
{
    size_t count = 0;
    for (const console_cmd_plugin_desc_t *it = plugin_array_begin(); it != plugin_array_end(); ++it) {
        count++;
    }
    return count;
}

esp_err_t console_cmd_plugin_foreach(console_cmd_plugin_cb_t cb, void *ctx)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (const console_cmd_plugin_desc_t *it = plugin_array_begin(); it != plugin_array_end(); ++it) {
        if (!cb(it, ctx)) {
            break;
        }
    }
    return ESP_OK;
}

esp_err_t console_cmd_register_plugin(const char *plugin_name)
{
    if (plugin_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (const console_cmd_plugin_desc_t *it = plugin_array_begin(); it != plugin_array_end(); ++it) {
        if (it->name == NULL || strcmp(it->name, plugin_name) != 0) {
            continue;
        }
        if (it->plugin_regd_fn == NULL) {
            return ESP_OK;
        }
        esp_err_t ret = it->plugin_regd_fn();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register plugin '%s': %s", it->name, esp_err_to_name(ret));
        }
        return ret;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t console_cmd_start(void)
{
    if (s_repl == NULL) {
        ESP_LOGE(TAG, "console_cmd_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    return esp_console_start_repl(s_repl);
}
