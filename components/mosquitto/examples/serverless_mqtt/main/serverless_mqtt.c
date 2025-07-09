/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_check.h"
#include "esp_sleep.h"
#include "mosq_broker.h"
#include "peer_impl.h"

#define ALIGN(size) (((size) + 3U) & ~(3U))

typedef struct message_wrap {
    uint16_t topic_len;
    uint16_t data_len;
    char data[];
} __attribute__((packed)) message_wrap_t;

static const char *TAG = "serverless_mqtt";

static esp_mqtt_client_handle_t s_local_mqtt = NULL;

char *wifi_get_ipv4(wifi_interface_t interface);
esp_err_t wifi_connect(void);
static esp_err_t create_local_client(void);
static esp_err_t create_local_broker(void);

esp_err_t peer_init(on_peer_recv_t cb);

static void peer_recv(const char *data, size_t size)
{
    if (s_local_mqtt) {
        message_wrap_t *message = (message_wrap_t *)data;
        int topic_len = message->topic_len;
        int payload_len = message->data_len;
        int topic_len_aligned = ALIGN(topic_len);
        char *topic = message->data;
        char *payload = message->data + topic_len_aligned;
        if (topic_len + topic_len_aligned + 4 > size) {
            ESP_LOGE(TAG, "Received invalid message");
            return;
        }
        ESP_LOGI(TAG, "forwarding remote message: topic:%s", topic);
        ESP_LOGI(TAG, "forwarding remote message: payload:%.*s", payload_len, payload);
        esp_mqtt_client_publish(s_local_mqtt, topic, payload, payload_len, 0, 0);
    }

}

void app_main(void)
{
    __attribute__((__unused__)) esp_err_t ret;
    ESP_GOTO_ON_ERROR(wifi_connect(), err, TAG, "Failed to initialize WiFi");
    ESP_GOTO_ON_ERROR(create_local_broker(), err, TAG, "Failed to create local broker");
    ESP_GOTO_ON_ERROR(peer_init(peer_recv), err, TAG, "Failed to init peer library");
    ESP_GOTO_ON_ERROR(create_local_client(), err, TAG, "Failed to create forwarding mqtt client");
    ESP_LOGI(TAG, "Everything is ready, exiting main task");
    return;
err:
    ESP_LOGE(TAG, "Non recoverable error, going to sleep for some time (random, max 20s)");
    esp_deep_sleep(1000000LL * (esp_random() % 20));
}

static void local_handler(void *args, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "local client connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "local client disconnected");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "local client error");
        break;
    default:
        ESP_LOGI(TAG, "local client event id:%d", (int)id);
        break;
    }
}

static esp_err_t create_local_client(void)
{
    esp_err_t ret = ESP_OK;
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .broker.address.hostname = wifi_get_ipv4(WIFI_IF_AP),
        .broker.address.port = CONFIG_EXAMPLE_MQTT_BROKER_PORT,
        .task.stack_size = CONFIG_EXAMPLE_MQTT_CLIENT_STACK_SIZE,
        .credentials.client_id = "local_mqtt"
    };
    s_local_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    ESP_GOTO_ON_FALSE(s_local_mqtt, ESP_ERR_NO_MEM, err, TAG, "Failed to create mqtt client");
    ESP_GOTO_ON_ERROR(esp_mqtt_client_register_event(s_local_mqtt, ESP_EVENT_ANY_ID, local_handler, NULL),
                      err, TAG, "Failed to register mqtt event handler");
    ESP_GOTO_ON_ERROR(esp_mqtt_client_start(s_local_mqtt), err, TAG, "Failed to start mqtt client");

    return ESP_OK;
err:
    esp_mqtt_client_destroy(s_local_mqtt);
    s_local_mqtt = NULL;
    return ret;
}

static void handle_message(char *client, char *topic, char *payload, int len, int qos, int retain)
{
    if (client && strcmp(client, "local_mqtt") == 0) {
        // This is our little local client -- do not forward
        return;
    }
    ESP_LOGI(TAG, "handle_message topic:%s", topic);
    ESP_LOGI(TAG, "handle_message data:%.*s", len, payload);
    ESP_LOGI(TAG, "handle_message qos=%d, retain=%d", qos, retain);
    if (s_local_mqtt) {
        static char *s_buffer = NULL;
        static size_t s_buffer_len = 0;
        if (s_buffer == NULL || s_buffer_len == 0) {
            peer_get_buffer(&s_buffer, &s_buffer_len);
            if (s_buffer == NULL || s_buffer_len == 0) {
                return;
            }
        }
        int topic_len = strlen(topic) + 1; // null term
        int topic_len_aligned = ALIGN(topic_len);
        int total_msg_len = 2 + 2 /* msg_wrap header */ + topic_len_aligned + len;
        if (total_msg_len > s_buffer_len) {
            ESP_LOGE(TAG, "Fail to forward, message too long");
            return;
        }
        message_wrap_t *message = (message_wrap_t *)s_buffer;
        message->topic_len = topic_len;
        message->data_len = len;

        memcpy(s_buffer + 4, topic, topic_len);
        memcpy(s_buffer + 4 + topic_len_aligned, payload, len);
        peer_send(s_buffer, total_msg_len);
    }
}

static void broker_task(void *ctx)
{
    struct mosq_broker_config config = { .host = wifi_get_ipv4(WIFI_IF_AP), .port = CONFIG_EXAMPLE_MQTT_BROKER_PORT, .handle_message_cb = handle_message };
    mosq_broker_run(&config);
    vTaskDelete(NULL);
}

static esp_err_t create_local_broker(void)
{
    return xTaskCreate(broker_task, "mqtt_broker_task", 1024 * 32, NULL, 5, NULL) == pdTRUE ?
           ESP_OK : ESP_FAIL;
}
