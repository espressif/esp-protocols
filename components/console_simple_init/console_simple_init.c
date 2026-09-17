/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "console_simple_init.h"


static esp_console_repl_t *s_repl = NULL;
static const char *TAG = "console_simple_init";

esp_err_t console_cmd_init(void)
{
    if (s_repl != NULL) {
        ESP_LOGW(TAG, "console already initialized");
        return ESP_OK;
    }

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    // install console REPL environment
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    return esp_console_new_repl_uart(&hw_config, &repl_config, &s_repl);

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &s_repl);

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &s_repl);

#else
#error Unsupported console type
#endif

}

esp_err_t console_cmd_user_register(const char *user_cmd, esp_console_cmd_func_t do_user_cmd)
{
    const esp_console_cmd_t cmd = {
        .command = user_cmd,
        .help = "User defined command",
        .hint = NULL,
        .func = do_user_cmd,
    };

    esp_err_t ret = esp_console_cmd_register(&cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to register user cmd '%s': %s",
                 user_cmd ? user_cmd : "(null)", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t console_cmd_all_register(void)
{
    extern const console_cmd_plugin_desc_t _console_cmd_array_start;
    extern const console_cmd_plugin_desc_t _console_cmd_array_end;

    ESP_LOGI(TAG, "List of Console commands:");
    for (const console_cmd_plugin_desc_t *it = &_console_cmd_array_start; it != &_console_cmd_array_end; ++it) {
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

esp_err_t console_cmd_start(void)
{
    if (s_repl == NULL) {
        ESP_LOGE(TAG, "console_cmd_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }
    return esp_console_start_repl(s_repl);
}
