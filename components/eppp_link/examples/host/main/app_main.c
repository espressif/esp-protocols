/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "eppp_link.h"
#include "esp_log.h"
#include "esp_check.h"
#include "mqtt_client.h"
#include "console_ping.h"

void register_iperf(void);

static const char *TAG = "eppp_host_example";

#if CONFIG_EXAMPLE_MQTT
static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
#endif // MQTT

void station_over_eppp_channel(void *arg);

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Sets up the default EPPP-connection
     */
    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
#if CONFIG_EPPP_LINK_DEVICE_SPI
    config.transport = EPPP_TRANSPORT_SPI;
    config.spi.is_master = true;
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
#elif CONFIG_EPPP_LINK_DEVICE_ETH
    config.transport = EPPP_TRANSPORT_ETHERNET;
#else
    config.transport = EPPP_TRANSPORT_SDIO;
    config.sdio.is_host = true;
#endif
    esp_netif_t *eppp_netif = eppp_connect(&config);
    if (eppp_netif == NULL) {
        ESP_LOGE(TAG, "Failed to connect");
        return ;
    }
    // Setup global DNS
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = esp_netif_htonl(CONFIG_EXAMPLE_GLOBAL_DNS);
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(eppp_netif, ESP_NETIF_DNS_MAIN, &dns));

    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());

#if CONFIG_EXAMPLE_IPERF
    register_iperf();

    printf("\n =======================================================\n");
    printf(" |       Steps to Test EPPP-host bandwidth             |\n");
    printf(" |                                                     |\n");
    printf(" |  1. Wait for the ESP32 to get an IP                 |\n");
    printf(" |  2. Server: 'iperf -u -s -i 3' (on host)            |\n");
    printf(" |  3. Client: 'iperf -u -c SERVER_IP -t 60 -i 3'      |\n");
    printf(" |                                                     |\n");
    printf(" =======================================================\n\n");

#endif // CONFIG_EXAMPLE_IPERF

    // Register the ping command
    ESP_ERROR_CHECK(console_cmd_ping_register());
    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());

#ifdef CONFIG_EXAMPLE_WIFI_OVER_EPPP_CHANNEL
    station_over_eppp_channel(eppp_netif);
#endif
#if CONFIG_EXAMPLE_MQTT
    mqtt_app_start();
#endif
}
