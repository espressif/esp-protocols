/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file main.c
 * @brief Main application for ESP32 remote console over WebSocket.
 *
 * This application initializes a WebSocket client, redirects standard I/O to a WebSocket
 * via a VFS driver, and runs a console REPL for remote command execution.
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_console.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "esp_websocket_client.h"
#include "websocket_client_vfs.h"
#include "cmd_nvs.h"
#include "console_simple_init.h"

static const char *TAG = "remote_console";
static const char *DEFAULT_WS_URI = "ws://192.168.50.231:8080";

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static esp_websocket_client_handle_t websocket_app_init(void);
static void websocket_app_exit(esp_websocket_client_handle_t client);
static void run_console_task(void);
static void console_task(void* arg);
static void vfs_exit(FILE* websocket_io);
static FILE* vfs_init(void);

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    esp_websocket_client_handle_t client = websocket_app_init();
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize websocket client");
        return;
    }
    ESP_LOGI(TAG, "Websocket client initialized");

    FILE* websocket_io = vfs_init();
    if (websocket_io == NULL) {
        ESP_LOGE(TAG, "Failed to open websocket I/O file");
        return;
    }

    run_console_task();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vfs_exit(websocket_io);
    websocket_app_exit(client);
}

static esp_websocket_client_handle_t websocket_app_init(void)
{
    websocket_client_vfs_config_t config = {
        .base_path = "/websocket",
        .send_timeout_ms = 10000,
        .recv_timeout_ms = 10000,
        .recv_buffer_size = 256,
        .fallback_stdout = stdout
    };
    ESP_ERROR_CHECK(websocket_client_vfs_register(&config));

    esp_websocket_client_config_t websocket_cfg = {};
    //websocket_cfg.uri = "ws://192.168.50.231:8080";
    websocket_cfg.uri = DEFAULT_WS_URI;
    websocket_cfg.reconnect_timeout_ms = 1000;
    websocket_cfg.network_timeout_ms = 10000;

    ESP_LOGI(TAG, "Connecting to %s...", websocket_cfg.uri);

    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);
    websocket_client_vfs_add_client(client, 0);

    return client;
}

static void websocket_app_exit(esp_websocket_client_handle_t client)
{
    esp_websocket_client_close(client, portMAX_DELAY);
    ESP_LOGI(TAG, "Websocket Stopped");
    esp_websocket_client_destroy(client);
}


/**
 * @brief Initialize VFS for WebSocket I/O redirection.
 * @return FILE pointer for WebSocket I/O, or NULL on failure.
 */
static FILE* vfs_init(void)
{
    FILE *websocket_io = fopen("/websocket/0", "r+");
    if (!websocket_io) {
        ESP_LOGE(TAG, "Failed to open websocket I/O file");
        return NULL;
    }

    stdin = websocket_io;
    stdout = websocket_io;
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    _GLOBAL_REENT->_stdin = websocket_io;
    _GLOBAL_REENT->_stdout = websocket_io;
    _GLOBAL_REENT->_stderr = websocket_io;

    return websocket_io;
}

/**
 * @brief Clean up VFS resources.
 * @param websocket_io FILE pointer for WebSocket I/O.
 */
static void vfs_exit(FILE* websocket_io)
{
    if (websocket_io) {
        fclose(websocket_io);
        websocket_io = NULL;
    }
}

/**
 * @brief WebSocket event handler.
 * @param handler_args User-defined arguments.
 * @param base Event base.
 * @param event_id Event ID.
 * @param event_data Event data.
 */
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Websocket connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Websocket disconnected");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x08 && data->data_len == 2) {
            ESP_LOGI(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "Websocket error");
        break;
    }

    websocket_client_vfs_event_handler(data->client, event_id, event_data);
}


static void run_console_task(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreate(console_task, "console_task", 16 * 1024, NULL, 5, NULL);
}


static void console_task(void* arg)
{
    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());
    ESP_ERROR_CHECK(console_cmd_all_register());

    // start console REPL
    ESP_ERROR_CHECK(console_cmd_start());

    while (true) {
        //fprintf(websocket_io, "From: %s(%d)\n", __func__, __LINE__);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    vTaskDelete(NULL);
}
