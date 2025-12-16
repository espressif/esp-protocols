/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "mosq_broker.h"
#include "protocol_examples_common.h"

__attribute__((unused)) const static char *TAG = "mqtt_broker";

/* Basic auth credentials for the example */
#define EXAMPLE_USERNAME "testuser"
#define EXAMPLE_PASSWORD "testpass"

#if CONFIG_EXAMPLE_BROKER_USE_BASIC_AUTH
/* Connection callback to validate username/password */
static int example_connect_callback(const char *client_id, const char *username, const char *password, int password_len)
{
    ESP_LOGI(TAG, "Connection attempt from client_id='%s', username='%s'", client_id, username ? username : "(none)");

    /* Check if username is provided */
    if (!username) {
        ESP_LOGW(TAG, "Connection rejected: no username provided");
        return 1; /* Reject connection */
    }

    /* Check if password is provided */
    if (!password) {
        ESP_LOGW(TAG, "Connection rejected: no password provided");
        return 1; /* Reject connection */
    }

    /* Validate username */
    if (strcmp(username, EXAMPLE_USERNAME) != 0) {
        ESP_LOGW(TAG, "Connection rejected: invalid username '%s'", username);
        return 1; /* Reject connection */
    }

    /* Validate password */
    if (strcmp(password, EXAMPLE_PASSWORD) != 0) {
        ESP_LOGW(TAG, "Connection rejected: invalid password");
        return 1; /* Reject connection */
    }

    ESP_LOGI(TAG, "Connection accepted for client_id='%s', username='%s'", client_id, username);
    return 0; /* Accept connection */
}
#endif /* CONFIG_EXAMPLE_BROKER_USE_BASIC_AUTH */

#if CONFIG_EXAMPLE_BROKER_WITH_TLS
extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
extern const unsigned char serverkey_start[] asm("_binary_serverkey_pem_start");
extern const unsigned char serverkey_end[]   asm("_binary_serverkey_pem_end");
extern const char cacert_start[] asm("_binary_cacert_pem_start");
extern const char cacert_end[]   asm("_binary_cacert_pem_end");
#endif


#if CONFIG_EXAMPLE_BROKER_RUN_LOCAL_MQTT_CLIENT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        vTaskDelay(pdMS_TO_TICKS(1000));    // wait a second for the broker to start
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
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
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(struct mosq_broker_config *config)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = "127.0.0.1",
#if CONFIG_EXAMPLE_BROKER_WITH_TLS
        .broker.address.transport = MQTT_TRANSPORT_OVER_SSL,
        .broker.verification.certificate = cacert_start,
        .broker.verification.certificate_len = cacert_end - cacert_start,
#else
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
#endif
        .broker.address.port = config->port,
#if CONFIG_EXAMPLE_BROKER_USE_BASIC_AUTH
        .credentials.username = "EXAMPLE_USERNAME",
        .credentials.authentication.password = EXAMPLE_PASSWORD,
#endif
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
#endif // CONFIG_EXAMPLE_BROKER_RUN_LOCAL_MQTT_CLIENT

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    struct mosq_broker_config config = {
        .host = CONFIG_EXAMPLE_BROKER_HOST,
        .port = CONFIG_EXAMPLE_BROKER_PORT,
        .tls_cfg = NULL,
#if CONFIG_EXAMPLE_BROKER_USE_BASIC_AUTH
        .handle_connect_cb = example_connect_callback,
#else
        .handle_connect_cb = NULL,
#endif
    };

#if CONFIG_EXAMPLE_BROKER_RUN_LOCAL_MQTT_CLIENT
    mqtt_app_start(&config);
#endif

#if CONFIG_EXAMPLE_BROKER_WITH_TLS
    esp_tls_cfg_server_t tls_cfg = {
        .servercert_buf = servercert_start,
        .servercert_bytes = servercert_end - servercert_start,
        .serverkey_buf = serverkey_start,
        .serverkey_bytes = serverkey_end - serverkey_start,
    };
    config.tls_cfg = &tls_cfg;
#endif

    // broker continues to run in this task
    mosq_broker_run(&config);
}
