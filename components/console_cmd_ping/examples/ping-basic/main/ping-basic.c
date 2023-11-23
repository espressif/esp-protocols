/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "console_ping.h"


void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init();   //Initialize NVS
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());

    // Register ping command
    ESP_ERROR_CHECK(console_cmd_ping_register());

    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());

}
