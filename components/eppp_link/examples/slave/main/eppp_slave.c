/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "eppp_link.h"
#include "inttypes.h"

static const char *TAG = "eppp_slave";

#if defined(CONFIG_SOC_WIFI_SUPPORTED) && !defined(CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL)

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "event_handler: event_base=%s event_id=%" PRIi32, event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI start event");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void init_network_interface(void)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
#else

// If the SoC does not have WiFi capabilities, we can initialize a different network interface, this function is a placeholder for that purpose.
// This function is also a no-op if EXAMPLE_WIFI_OVER_EPPP_CHANNEL==1, since the Wi-Fi network interface will live on the other peer (on the host side).
void init_network_interface(void)
{
}

#endif // SoC WiFi capable chip || WiFi over EPPP channel

void station_over_eppp_channel(void *arg);

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
    init_network_interface();   // WiFi station if withing SoC capabilities (otherwise a placeholder)

    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
#if CONFIG_EPPP_LINK_DEVICE_SPI
    config.transport = EPPP_TRANSPORT_SPI;
    config.spi.is_master = false;
    config.spi.host = CONFIG_EXAMPLE_SPI_HOST;
    config.spi.mosi = CONFIG_EXAMPLE_SPI_MOSI_PIN;
    config.spi.miso = CONFIG_EXAMPLE_SPI_MISO_PIN;
    config.spi.sclk = CONFIG_EXAMPLE_SPI_SCLK_PIN;
    config.spi.cs = CONFIG_EXAMPLE_SPI_CS_PIN;
    config.spi.intr = CONFIG_EXAMPLE_SPI_INTR_PIN;
    config.spi.freq = CONFIG_EXAMPLE_SPI_FREQUENCY;
#elif CONFIG_EPPP_LINK_DEVICE_UART
    config.transport = EPPP_TRANSPORT_UART;
    config.uart.tx_io = CONFIG_EXAMPLE_UART_TX_PIN;
    config.uart.rx_io = CONFIG_EXAMPLE_UART_RX_PIN;
    config.uart.baud = CONFIG_EXAMPLE_UART_BAUDRATE;
#elif CONFIG_EPPP_LINK_DEVICE_SDIO
    config.transport = EPPP_TRANSPORT_SDIO;
#endif // transport device

    esp_netif_t *eppp_netif = eppp_listen(&config);
    if (eppp_netif == NULL) {
        ESP_LOGE(TAG, "Failed to setup connection");
        return ;
    }
#ifdef CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL
    station_over_eppp_channel(eppp_netif);
#else
    ESP_ERROR_CHECK(esp_netif_napt_enable(eppp_netif));
#endif // CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL
}
