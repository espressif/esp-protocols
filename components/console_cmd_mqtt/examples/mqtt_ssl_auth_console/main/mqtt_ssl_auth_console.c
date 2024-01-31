/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <netdb.h>
#include "console_mqtt.h"
#include "protocol_examples_common.h"

// Certs for mqtts://test.mosquitto.org:8884
extern const uint8_t g_client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t g_client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t g_client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t g_client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t g_broker_cert_pem_start[] asm("_binary_mosquitto_org_pem_start");
extern const uint8_t g_broker_cert_pem_end[] asm("_binary_mosquitto_org_pem_end");


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

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * ${IDF_PATH}/examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());
    ESP_ERROR_CHECK(console_cmd_all_register());

    set_mqtt_client_cert(g_client_cert_pem_start, g_client_cert_pem_end);
    set_mqtt_client_key(g_client_key_pem_start, g_client_key_pem_end);
    set_mqtt_broker_certs(g_broker_cert_pem_start, g_broker_cert_pem_end);

    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());

}
