/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_check.h"
#include "juice/juice.h"
#include "cJSON.h"
#include "peer_impl.h"

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

static const char *TAG = "serverless_mqtt" OUR_PEER;
static char s_buffer[MAX_BUFFER_SIZE];
static EventGroupHandle_t s_state = NULL;
static juice_agent_t *s_agent = NULL;
static cJSON *s_peer_desc_json = NULL;
static char *s_peer_desc = NULL;
static esp_mqtt_client_handle_t s_local_mqtt = NULL;
static on_peer_recv_t s_on_recv = NULL;

char *wifi_get_ipv4(wifi_interface_t interface);
static esp_err_t sync_peers(void);
static esp_err_t create_candidates(void);

void peer_get_buffer(char ** buffer, size_t *buffer_len)
{
    if (buffer && buffer_len) {
        *buffer = s_buffer;
        *buffer_len = MAX_BUFFER_SIZE;
    }
}

void peer_send(char* data, size_t size)
{
    juice_send(s_agent, data, size);
}

esp_err_t peer_init(on_peer_recv_t cb)
{
    esp_err_t ret = ESP_FAIL;
    ESP_GOTO_ON_FALSE(cb, ESP_ERR_INVALID_ARG, err, TAG, "Invalid peer receive callback");
    s_on_recv = cb;
    ESP_GOTO_ON_ERROR(create_candidates(), err, TAG, "Failed to create juice candidates");
    ESP_GOTO_ON_ERROR(sync_peers(), err, TAG, "Failed to sync with the other peer");
    EventBits_t bits = xEventGroupWaitBits(s_state, PEER_FAIL | PEER_CONNECTED, pdFALSE, pdFALSE, pdMS_TO_TICKS(90000));
    if (bits & PEER_CONNECTED) {
        ESP_LOGI(TAG, "Peer is connected!");
        return ESP_OK;
    }
err:
    ESP_LOGE(TAG, "Failed to init peer");
    return ret;
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



static void juice_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr)
{
    if (s_local_mqtt) {
        s_on_recv(data, size);
    } else {
        ESP_LOGI(TAG, "No local mqtt client, dropping data");
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
