/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal sanity-check app for usrsctp: brings the userspace SCTP
 * stack up and tears it down. Used by .github/workflows/build.yml to
 * verify the component compiles and links across supported ESP-IDF
 * targets.
 */

#include <stdio.h>
#include "esp_log.h"
#include "usrsctp.h"

static const char *TAG = "usrsctp_check";

void app_main(void)
{
    usrsctp_init(0, NULL, NULL);
    ESP_LOGI(TAG, "usrsctp_init() OK");

    usrsctp_finish();
    ESP_LOGI(TAG, "usrsctp build check complete");
}
