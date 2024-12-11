/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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
#include "juice/juice.h"
#include "cJSON.h"

#if defined(CONFIG_EXAMPLE_SERVERLESS_ROLE_PEER1)
#define OUR_PEER "1"
#define THEIR_PEER "2"
#elif defined(CONFIG_EXAMPLE_SERVERLESS_ROLE_PEER2)
#define OUR_PEER "2"
#define THEIR_PEER "1"
#endif

#define PEER_SYNC0 BIT(0)
#define PEER_SYNC1 BIT(1)
#define PEER_SYNC2 BIT(2)
#define PEER_FAIL  BIT(3)
#define PEER_GATHER_DONE  BIT(4)
#define PEER_DESC_PUBLISHED  BIT(5)
#define PEER_CONNECTED  BIT(6)

#define SYNC_BITS (PEER_SYNC1 | PEER_SYNC2 | PEER_FAIL)

#define PUBLISH_SYNC_TOPIC CONFIG_EXAMPLE_MQTT_SYNC_TOPIC OUR_PEER
#define SUBSCRIBE_SYNC_TOPIC CONFIG_EXAMPLE_MQTT_SYNC_TOPIC THEIR_PEER
#define MAX_BUFFER_SIZE JUICE_MAX_SDP_STRING_LEN

typedef struct message_wrap {
    uint16_t topic_len;
    uint16_t data_len;
    char data[];
} __attribute__((packed)) message_wrap_t;

static const char *TAG = "serverless_mqtt" OUR_PEER;
static char s_buffer[MAX_BUFFER_SIZE];
static EventGroupHandle_t s_state = NULL;
static juice_agent_t *s_agent = NULL;
static cJSON *s_peer_desc_json = NULL;
static char *s_peer_desc = NULL;
static esp_mqtt_client_handle_t s_local_mqtt = NULL;

char *wifi_get_ipv4(wifi_interface_t interface);
esp_err_t wifi_connect(void);
static esp_err_t sync_peers(void);
static esp_err_t create_candidates(void);
static esp_err_t create_local_client(void);
static esp_err_t create_local_broker(void);

void app_main(void)
{
    __attribute__((__unused__)) esp_err_t ret;
    ESP_GOTO_ON_ERROR(wifi_connect(), err, TAG, "Failed to initialize WiFi");
    ESP_GOTO_ON_ERROR(create_local_broker(), err, TAG, "Failed to create local broker");
    ESP_GOTO_ON_ERROR(create_candidates(), err, TAG, "Failed to create juice candidates");
    ESP_GOTO_ON_ERROR(sync_peers(), err, TAG, "Failed to sync with the other peer");
    EventBits_t bits = xEventGroupWaitBits(s_state, PEER_FAIL | PEER_CONNECTED, pdFALSE, pdFALSE, pdMS_TO_TICKS(90000));
    if (bits & PEER_CONNECTED) {
        ESP_LOGI(TAG, "Peer is connected!");
        ESP_GOTO_ON_ERROR(create_local_client(), err, TAG, "Failed to create forwarding mqtt client");
        ESP_LOGI(TAG, "Everything is ready, exiting main task");
        return;
    }
err:
    ESP_LOGE(TAG, "Non recoverable error, going to sleep for some time (random, max 20s)");
    esp_deep_sleep(1000000LL * (esp_random() % 20));
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (esp_mqtt_client_subscribe(client, SUBSCRIBE_SYNC_TOPIC, 1) < 0) {
            ESP_LOGE(TAG, "Failed to subscribe to the sync topic");
        }
        xEventGroupSetBits(s_state, PEER_SYNC0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupSetBits(s_state, PEER_FAIL);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (s_state == NULL || memcmp(event->topic, SUBSCRIBE_SYNC_TOPIC, event->topic_len) != 0) {
            break;
        }
        EventBits_t bits = xEventGroupGetBits(s_state);
        if (event->data_len > 1 && s_agent) {
            cJSON *root = cJSON_Parse(event->data);
            if (root == NULL) {
                break;
            }
            cJSON *desc = cJSON_GetObjectItem(root, "desc");
            if (desc == NULL) {
                cJSON_Delete(root);
                break;
            }
            printf("desc->valuestring:%s\n", desc->valuestring);
            juice_set_remote_description(s_agent, desc->valuestring);
            char cand_name[] = "cand0";
            while (true) {
                cJSON *cand = cJSON_GetObjectItem(root, cand_name);
                if (cand == NULL) {
                    break;
                }
                printf("%s: cand->valuestring:%s\n", cand_name, cand->valuestring);
                juice_add_remote_candidate(s_agent, cand->valuestring);
                cand_name[4]++;
            }
            cJSON_Delete(root);
            xEventGroupSetBits(s_state, PEER_DESC_PUBLISHED); // this will complete the sync process
            // and destroy the mqtt client
        }
#ifdef CONFIG_EXAMPLE_SERVERLESS_ROLE_PEER1
        if (event->data_len == 1 && event->data[0] == '1' && (bits & PEER_SYNC2) == 0) {
            if (esp_mqtt_client_publish(client, PUBLISH_SYNC_TOPIC, "2", 1, 1, 0) >= 0) {
                xEventGroupSetBits(s_state, PEER_SYNC2);
            } else {
                xEventGroupSetBits(s_state, PEER_FAIL);
            }
        }
#else
        if (event->data_len == 1 && event->data[0] == '0' && (bits & PEER_SYNC1) == 0) {
            if (esp_mqtt_client_publish(client, PUBLISH_SYNC_TOPIC, "1", 1, 1, 0) >= 0) {
                xEventGroupSetBits(s_state, PEER_SYNC1);
            }  else {
                xEventGroupSetBits(s_state, PEER_FAIL);
            }
        } else if (event->data_len == 1 && event->data[0] == '2' && (bits & PEER_SYNC2) == 0) {
            xEventGroupSetBits(s_state, PEER_SYNC2);
        }
#endif
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        xEventGroupSetBits(s_state, PEER_FAIL);
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static esp_err_t sync_peers(void)
{
    esp_err_t ret = ESP_OK;
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_EXAMPLE_MQTT_BROKER_URI,
        .task.stack_size = CONFIG_EXAMPLE_MQTT_CLIENT_STACK_SIZE,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_GOTO_ON_FALSE(client, ESP_ERR_NO_MEM, err, TAG, "Failed to create mqtt client");
    ESP_GOTO_ON_ERROR(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL),
                      err, TAG, "Failed to register mqtt event handler");
    ESP_GOTO_ON_ERROR(esp_mqtt_client_start(client), err, TAG, "Failed to start mqtt client");
    ESP_GOTO_ON_FALSE(xEventGroupWaitBits(s_state, PEER_SYNC0, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000)),
                      ESP_FAIL, err, TAG, "Failed to connect to the sync broker");
    ESP_LOGI(TAG, "Waiting for the other peer...");
    const int max_sync_retry = 60;
    int retry = 0;
    while (true) {
        EventBits_t bits = xEventGroupWaitBits(s_state, SYNC_BITS, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & PEER_SYNC2) {
            break;
        }
        if (bits & PEER_SYNC1) {
            continue;
        }
        ESP_GOTO_ON_FALSE((bits & PEER_FAIL) == 0, ESP_FAIL, err, TAG, "Failed to sync with the other peer");
        ESP_GOTO_ON_FALSE(retry++ < max_sync_retry, ESP_FAIL, err, TAG, "Failed to sync after %d seconds", retry);
#ifdef CONFIG_EXAMPLE_SERVERLESS_ROLE_PEER1
        ESP_RETURN_ON_FALSE(esp_mqtt_client_publish(client, PUBLISH_SYNC_TOPIC, "0", 1, 1, 0) >= 0,
                            ESP_FAIL, TAG, "Failed to publish mqtt message");
#endif
    }
    ESP_LOGI(TAG, "Sync done");
    ESP_RETURN_ON_FALSE(esp_mqtt_client_publish(client, PUBLISH_SYNC_TOPIC, s_peer_desc, 0, 1, 0) >= 0,
                        ESP_FAIL, TAG, "Failed to publish peer's description");
    ESP_LOGI(TAG, "Waiting for the other peer description and candidates...");
    ESP_GOTO_ON_FALSE(xEventGroupWaitBits(s_state, PEER_DESC_PUBLISHED, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000)),
                      ESP_FAIL, err, TAG, "Timeout in waiting for the other peer candidates");
err:
    free(s_peer_desc);
    esp_mqtt_client_destroy(client);
    return ret;
}

static void juice_state(juice_agent_t *agent, juice_state_t state, void *user_ptr)
{
    ESP_LOGI(TAG, "JUICE state change: %s", juice_state_to_string(state));
    if (state == JUICE_STATE_CONNECTED) {
        xEventGroupSetBits(s_state, PEER_CONNECTED);
    } else if (state == JUICE_STATE_FAILED || state == JUICE_STATE_DISCONNECTED) {
        esp_restart();
    }
}

static void juice_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr)
{
    static uint8_t cand_nr = 0;
    if (s_peer_desc_json && cand_nr < 10) { // supporting only 10 candidates
        char cand_name[] = "cand0";
        cand_name[4] += cand_nr++;
        cJSON_AddStringToObject(s_peer_desc_json, cand_name, sdp);
    }
}

static void juice_gathering_done(juice_agent_t *agent, void *user_ptr)
{
    ESP_LOGI(TAG, "Gathering done");
    if (s_state) {
        xEventGroupSetBits(s_state, PEER_GATHER_DONE);
    }
}

#define ALIGN(size) (((size) + 3U) & ~(3U))

static void juice_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr)
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

static esp_err_t create_candidates(void)
{
    ESP_RETURN_ON_FALSE(s_state = xEventGroupCreate(), ESP_ERR_NO_MEM, TAG, "Failed to create state event group");
    s_peer_desc_json = cJSON_CreateObject();
    esp_err_t ret = ESP_OK;
    juice_set_log_level(JUICE_LOG_LEVEL_INFO);
    juice_config_t config = { .stun_server_host = CONFIG_EXAMPLE_STUN_SERVER,
                              .bind_address = wifi_get_ipv4(WIFI_IF_STA),
                              .stun_server_port = 19302,
                              .cb_state_changed = juice_state,
                              .cb_candidate = juice_candidate,
                              .cb_gathering_done = juice_gathering_done,
                              .cb_recv = juice_recv,
                            };

    s_agent = juice_create(&config);
    ESP_RETURN_ON_FALSE(s_agent, ESP_FAIL, TAG, "Failed to create juice agent");
    ESP_GOTO_ON_FALSE(juice_get_local_description(s_agent, s_buffer, MAX_BUFFER_SIZE) == JUICE_ERR_SUCCESS,
                      ESP_FAIL, err, TAG, "Failed to get local description");
    ESP_LOGI(TAG, "desc: %s", s_buffer);
    cJSON_AddStringToObject(s_peer_desc_json, "desc", s_buffer);

    ESP_GOTO_ON_FALSE(juice_gather_candidates(s_agent) == JUICE_ERR_SUCCESS,
                      ESP_FAIL, err, TAG, "Failed to start gathering candidates");
    ESP_GOTO_ON_FALSE(xEventGroupWaitBits(s_state, PEER_GATHER_DONE, pdTRUE, pdTRUE, pdMS_TO_TICKS(30000)),
                      ESP_FAIL, err, TAG, "Failed to connect to the sync broker");
    s_peer_desc = cJSON_Print(s_peer_desc_json);
    ESP_LOGI(TAG, "desc: %s", s_peer_desc);
    cJSON_Delete(s_peer_desc_json);
    return ESP_OK;

err:
    juice_destroy(s_agent);
    s_agent = NULL;
    cJSON_Delete(s_peer_desc_json);
    s_peer_desc_json = NULL;
    return ret;
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
    if (client && strcmp(client, "local_mqtt") == 0 ) {
        // This is our little local client -- do not forward
        return;
    }
    ESP_LOGI(TAG, "handle_message topic:%s", topic);
    ESP_LOGI(TAG, "handle_message data:%.*s", len, payload);
    ESP_LOGI(TAG, "handle_message qos=%d, retain=%d", qos, retain);
    if (s_local_mqtt && s_agent) {
        int topic_len = strlen(topic) + 1; // null term
        int topic_len_aligned = ALIGN(topic_len);
        int total_msg_len = 2 + 2 /* msg_wrap header */ + topic_len_aligned + len;
        if (total_msg_len > MAX_BUFFER_SIZE) {
            ESP_LOGE(TAG, "Fail to forward, message too long");
            return;
        }
        message_wrap_t *message = (message_wrap_t *)s_buffer;
        message->topic_len = topic_len;
        message->data_len = len;

        memcpy(s_buffer + 4, topic, topic_len);
        memcpy(s_buffer + 4 + topic_len_aligned, payload, len);
        juice_send(s_agent, s_buffer, total_msg_len);
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
