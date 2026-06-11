/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal sanity-check app for libsrtp: initializes and shuts down libsrtp.
 * Used by .github/workflows/build.yml to verify the component compiles and
 * links across supported ESP-IDF targets.
 */

#include <stdio.h>
#include "esp_log.h"
#include "srtp2/srtp.h"

static const char *TAG = "libsrtp_check";

void app_main(void)
{
    srtp_err_status_t status = srtp_init();
    if (status == srtp_err_status_ok) {
        ESP_LOGI(TAG, "srtp_init() OK");
    } else {
        ESP_LOGE(TAG, "srtp_init() failed: %d", status);
    }
    srtp_shutdown();
    ESP_LOGI(TAG, "libsrtp build check complete");
}
