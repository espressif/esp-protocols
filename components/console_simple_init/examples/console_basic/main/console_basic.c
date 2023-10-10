/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "console_simple_init.h"

int do_user_cmd(int argc, char **argv)
{
    printf("Hello from user command\n");
    return 0;
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init();   //Initialize NVS
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());

    // Register user command
    ESP_ERROR_CHECK(console_cmd_user_register("user", do_user_cmd));

    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());

}
