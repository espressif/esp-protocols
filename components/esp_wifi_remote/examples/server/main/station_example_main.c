/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <esp_private/wifi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "eppp_link.h"
#include "esp_wifi_remote.h"

static const char *TAG = "sta2pppos";

esp_err_t server_init(void);

static esp_netif_t *s_ppp_netif;

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
//    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
//    config.transport = EPPP_TRANSPORT_UART;
//    s_ppp_netif = eppp_listen(&config);
//    if (s_ppp_netif == NULL) {
//        ESP_LOGE(TAG, "Failed to setup connection");
//        return ;
//    }

    server_init();
}
