/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "console_iperf.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());

    ESP_ERROR_CHECK(console_cmd_all_register());

    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());

}
