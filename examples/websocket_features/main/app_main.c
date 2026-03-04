/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_websocket_client.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ws_features";

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
#if WS_TRANSPORT_HEADER_CALLBACK_SUPPORT
    case WEBSOCKET_EVENT_HEADER_RECEIVED:
        /* New feature: inspect HTTP headers during the upgrade response. */
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_HEADER_RECEIVED: %.*s", data->data_len, data->data_ptr);
        break;
#endif
    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEFORE_CONNECT");
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA opcode=%d fin=%d payload_len=%d payload_offset=%d",
                 data->op_code, data->fin, data->payload_len, data->payload_offset);
        ESP_LOGI(TAG, "Payload chunk: %.*s", data->data_len, data->data_ptr);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WEBSOCKET_EVENT_ERROR type=%d status=%d tls_esp_err=0x%x sock_errno=%d",
                 data->error_handle.error_type,
                 data->error_handle.esp_ws_handshake_status_code,
                 data->error_handle.esp_tls_last_esp_err,
                 data->error_handle.esp_transport_sock_errno);
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED");
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
    default:
        break;
    }
}

static void websocket_feature_showcase(void)
{
    /*
     * This config intentionally enables and documents several modern features:
     *  - close-reconnect behavior
     *  - runtime ping and reconnect tuning
     *  - custom handshake headers
     *  - fragmented send helpers
     *  - pause/resume without destroying the worker task
     */
    esp_websocket_client_config_t ws_cfg = {
        .uri = CONFIG_WEBSOCKET_FEATURES_URI,
        .enable_close_reconnect = true,
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 4000,
        .network_timeout_ms = 8000,
        .ping_interval_sec = 8,
        .pingpong_timeout_sec = 20,
    };

    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize websocket client");
        return;
    }

    ESP_ERROR_CHECK(esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL));

    /* Add headers in two ways: batch set + key/value append API. */
    ESP_ERROR_CHECK(esp_websocket_client_set_headers(client,
                    "X-Example-Profile: feature-showcase\r\n"
                    "X-Trace-Id: boot-phase\r\n"));
    ESP_ERROR_CHECK(esp_websocket_client_append_header(client, "X-Client", "esp-protocols-example"));

    ESP_LOGI(TAG, "Connecting to %s", ws_cfg.uri);
    ESP_ERROR_CHECK(esp_websocket_client_start(client));

    for (int retry = 0; retry < 30 && !esp_websocket_client_is_connected(client); ++retry) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (esp_websocket_client_is_connected(client)) {
        const char *text_msg = "Feature showcase: single text frame";
        ESP_LOGI(TAG, "Sending text frame");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_text(client, text_msg, strlen(text_msg), pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);

        /* Send a fragmented text message in 3 pieces. */
        ESP_LOGI(TAG, "Sending fragmented text frame");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_text_partial(client, "fragment-1/", 11, pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_cont_msg(client, "fragment-2/", 11, pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_cont_msg(client, "fragment-3", 10, pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_fin(client, pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);

        /* Send a fragmented binary message. */
        const char bin_a[] = {0x01, 0x02, 0x03, 0x04};
        const char bin_b[] = {0x05, 0x06, 0x07, 0x08};
        ESP_LOGI(TAG, "Sending fragmented binary frame");
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_bin_partial(client, bin_a, sizeof(bin_a), pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_cont_msg(client, bin_b, sizeof(bin_b), pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_websocket_client_send_fin(client, pdMS_TO_TICKS(1000)) < 0 ? ESP_FAIL : ESP_OK);

        /* Demonstrate runtime tuning for new ping/reconnect controls. */
        ESP_LOGI(TAG, "Current ping interval: %u sec", (unsigned)esp_websocket_client_get_ping_interval_sec(client));
        ESP_ERROR_CHECK(esp_websocket_client_set_ping_interval_sec(client, 5));
        ESP_LOGI(TAG, "Updated ping interval: %u sec", (unsigned)esp_websocket_client_get_ping_interval_sec(client));

        ESP_LOGI(TAG, "Current reconnect timeout: %d ms", esp_websocket_client_get_reconnect_timeout(client));
        ESP_ERROR_CHECK(esp_websocket_client_set_reconnect_timeout(client, 3000));
        ESP_LOGI(TAG, "Updated reconnect timeout: %d ms", esp_websocket_client_get_reconnect_timeout(client));

        /*
         * Pause/resume keeps the websocket task alive but tears down the transport.
         * It is useful when your app changes endpoint/headers at runtime.
         */
        ESP_LOGI(TAG, "Pausing client");
        ESP_ERROR_CHECK(esp_websocket_client_pause(client));
        ESP_ERROR_CHECK(esp_websocket_client_set_uri(client, CONFIG_WEBSOCKET_FEATURES_URI));
        ESP_LOGI(TAG, "Resuming client with new temporary header");
        ESP_ERROR_CHECK(esp_websocket_client_resume(client, "X-Resume-Reason: config-refresh\r\n"));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Stopping client");
    ESP_ERROR_CHECK(esp_websocket_client_close(client, pdMS_TO_TICKS(1000)));
    ESP_ERROR_CHECK(esp_websocket_unregister_events(client, WEBSOCKET_EVENT_ANY, ws_event_handler));
    ESP_ERROR_CHECK(esp_websocket_client_destroy(client));
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Bring up network using helper from protocol_examples_common. */
    ESP_ERROR_CHECK(example_connect());

    websocket_feature_showcase();
}
