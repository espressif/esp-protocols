/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "console_simple_init.h"
#include "console_simple_init_priv.h"

static const char *TAG = "console_simple_init";

esp_err_t console_cmd_stop(void)
{
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 5, 0)
    ESP_LOGE(TAG, "console_cmd_stop() requires ESP-IDF >= 5.5");
    return ESP_ERR_NOT_SUPPORTED;
#else
    esp_console_repl_t *repl = console_cmd_get_repl();
    if (repl == NULL) {
        ESP_LOGE(TAG, "console_cmd_init() must be called first");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_console_stop_repl(repl);
    if (ret == ESP_OK) {
        console_cmd_internal_clear_repl();
    } else {
        ESP_LOGE(TAG, "Failed to stop console: %s", esp_err_to_name(ret));
    }
    return ret;
#endif
}
