/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "mosq_broker.h"
#include "protocol_examples_common.h"

const static char *TAG = "mqtt_broker";

#if CONFIG_EXAMPLE_BROKER_RUN_LOCAL_MQTT_CLIENT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
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
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


static void mqtt_app_start(void *ctx)
{
    struct mosq_broker_config *config = ctx;

    vTaskDelay(pdMS_TO_TICKS(1000));    // wait a second for the broker to start
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = config->host,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,    // we support only TCP transport now
        .broker.address.port = config->port,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    vTaskDelete(NULL);
}
#endif // CONFIG_EXAMPLE_BROKER_RUN_LOCAL_MQTT_CLIENT

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    struct mosq_broker_config config = { .host = NULL, .port = CONFIG_EXAMPLE_BROKER_PORT };
#if CONFIG_EXAMPLE_BROKER_USE_CONNECTED_NETIF
    esp_netif_ip_info_t ip;
    char bind_host[4 * 4];  // to hold IPv4 address
    ESP_ERROR_CHECK(esp_netif_get_ip_info(get_example_netif(), &ip));
    esp_ip4addr_ntoa(&ip.ip, bind_host, sizeof(bind_host));
    config.host = bind_host;
#else
    config.host = CONFIG_EXAMPLE_BROKER_HOST;
#endif

#if CONFIG_EXAMPLE_BROKER_RUN_LOCAL_MQTT_CLIENT
    xTaskCreate(mqtt_app_start, "mqtt_client", 4096, &config, 4, NULL);
#endif
    // broker continues to run in this task
    run_broker(&config);
}
