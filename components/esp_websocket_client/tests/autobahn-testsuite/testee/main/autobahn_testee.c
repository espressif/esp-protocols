/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_websocket_client.h"
#include "esp_transport_ws.h"
#include "cJSON.h"

#if CONFIG_IDF_TARGET_LINUX
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#endif

#define TAG "autobahn"
#if CONFIG_WEBSOCKET_URI_FROM_STDIN
// URI will be read from stdin at runtime
static char g_autobahn_server_uri[256] = {0};
#define AUTOBAHN_SERVER_URI g_autobahn_server_uri
#else
#define AUTOBAHN_SERVER_URI CONFIG_AUTOBAHN_SERVER_URI
#endif
#if CONFIG_IDF_TARGET_LINUX
#define BUFFER_SIZE         65536
#else
#define BUFFER_SIZE         4096
#endif
// Configure test range here:
// Category 1 (Framing):          Tests 1-16
// Category 2 (Ping/Pong):        Tests 17-27
// Category 3 (Reserved Bits):    Tests 28-34
// Category 4 (Opcodes):          Tests 35-44
// Category 5 (Fragmentation):    Tests 45-64
// Category 6 (UTF-8):            Tests 65-209
// Category 7 (Close Handshake):  Tests 210-246
// All tests:                     Tests 1-300
// Defaults if get_case_count fails
#define DEFAULT_START_CASE  1
#define DEFAULT_END_CASE    300

#if CONFIG_IDF_TARGET_LINUX
static sem_t test_done_sem_storage;
static sem_t *test_done_sem = NULL;
#else
static SemaphoreHandle_t test_done_sem = NULL;
#endif
static bool test_running = false;

// Global to store total case count fetched from server
static int g_total_cases = 0;

/* Messages are echoed chunk by chunk as they arrive instead of being reassembled, so memory use
 * does not depend on message size (a 64 KB reassembly buffer does not fit reliably on ESP32-S2).
 * True while the first chunk of a message has been echoed without FIN and its end is still pending. */
static bool s_echo_in_progress = false;
/* True while the remaining chunks of a frame that started inside a fragmented message are skipped */
static bool s_skip_frame = false;

/* Chunks are echoed while the peer is still sending the rest of the message, so the socket can stay
 * backed up for hundreds of milliseconds on a busy WiFi link, regardless of the chunk size. */
#define ECHO_SEND_TIMEOUT_MS 2000

static int echo_send(esp_websocket_client_handle_t client, uint8_t opcode, bool fin,
                     const char *payload, int len, TickType_t timeout)
{
    if (fin) {
        return esp_websocket_client_send_with_opcode(client, opcode, (const uint8_t *)payload, len, timeout);
    }
    switch (opcode) {
    case WS_TRANSPORT_OPCODES_TEXT:
        return esp_websocket_client_send_text_partial(client, payload, len, timeout);
    case WS_TRANSPORT_OPCODES_BINARY:
        return esp_websocket_client_send_bin_partial(client, payload, len, timeout);
    default:
        return esp_websocket_client_send_cont_msg(client, payload, len, timeout);
    }
}

/* ------------------------------------------------------------
 *  Low‑latency echo handler
 * ------------------------------------------------------------ */
static void websocket_event_handler(void *handler_args,
                                    esp_event_base_t base,
                                    int32_t event_id,
                                    void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    esp_websocket_client_handle_t client = (esp_websocket_client_handle_t)handler_args;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        test_running = true;
        s_echo_in_progress = false;
        s_skip_frame = false;
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        test_running = false;
        s_echo_in_progress = false;
        s_skip_frame = false;
        /* Do not signal test_done here: ERROR/DISCONNECTED can fire while the
         * client task still holds its lock mid-abort. Wait for FINISH so stop/destroy
         * does not race the task teardown (hangs on Linux host). */
        break;

    case WEBSOCKET_EVENT_DATA: {
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA: opcode=0x%02X len=%d fin=%d payload_len=%d offset=%d",
                 data->op_code, data->data_len, data->fin, data->payload_len, data->payload_offset);

        // Safety check: if not connected, don't process data
        if (!test_running || !esp_websocket_client_is_connected(client)) {
            ESP_LOGW(TAG, "Received data but not connected, ignoring");
            break;
        }

        /* ---- skip control frames ---- */
        if (data->op_code >= 0x08) {
            if (data->op_code == 0x09) {
                ESP_LOGD(TAG, "PING -> PONG auto-sent");
            }
            break;
        }

        if (data->op_code != WS_TRANSPORT_OPCODES_CONT && data->op_code != WS_TRANSPORT_OPCODES_TEXT &&
                data->op_code != WS_TRANSPORT_OPCODES_BINARY) {
            ESP_LOGW(TAG, "Unsupported opcode 0x%02X - skip", data->op_code);
            break;
        }

        // Safety check: validate data pointer before processing
        if (!data->data_ptr && data->data_len > 0) {
            ESP_LOGE(TAG, "NULL data pointer with non-zero length: %d", data->data_len);
            break;
        }

        /* A frame larger than the receive buffer is delivered as several events with an advancing
         * payload_offset, so a message starts at offset 0 of a TEXT/BINARY frame and ends with the
         * last chunk of a FIN frame. */
        bool end_of_frame = data->payload_offset + data->data_len >= data->payload_len;
        bool first = data->op_code != WS_TRANSPORT_OPCODES_CONT && data->payload_offset == 0;
        bool last = data->fin && end_of_frame;
        if (s_skip_frame) {
            s_skip_frame = !end_of_frame;
            break;
        }
        if (!first && !s_echo_in_progress) {
            ESP_LOGW(TAG, "Continuation without an echoed message start, skipping");
            break;
        }
        /* RFC 6455 5.4: a new data frame inside a fragmented message is a protocol error (case 5.18).
         * The client library does not fail the connection for it, and close() cannot be called from
         * this handler, so only avoid echoing an invalid frame sequence back by skipping every chunk
         * of the offending frame. */
        if (first && s_echo_in_progress) {
            ESP_LOGW(TAG, "New message (opcode 0x%02X) while a fragmented message is in progress, skipping",
                     data->op_code);
            s_skip_frame = !end_of_frame;
            break;
        }
        uint8_t send_opcode = first ? data->op_code : WS_TRANSPORT_OPCODES_CONT;
        const char *payload = data->data_ptr;
        size_t len = data->data_len;
        s_echo_in_progress = !last;

        // Check connection before attempting to send
        if (!esp_websocket_client_is_connected(client)) {
            ESP_LOGW(TAG, "Connection lost before echo, skipping");
            break;
        }

        int sent = -1;
        int attempt = 0;
        const TickType_t backoff[] = {1, 1, 1, 2, 4, 8};  // Shorter backoff for faster retry
        int64_t start = esp_timer_get_time();

        while (sent < 0 && esp_websocket_client_is_connected(client)) {
            sent = echo_send(client, send_opcode, last, (len > 0) ? payload : NULL, len,
                             pdMS_TO_TICKS(ECHO_SEND_TIMEOUT_MS));

            if (sent >= 0) {
                ESP_LOGD(TAG, "Echo sent successfully: %d bytes", sent);
                break;
            }
            ESP_LOGW(TAG,
                     "echo send retry: opcode=0x%02X len=%zu fin=%d attempt=%d sent=%d",
                     send_opcode, len, last, attempt + 1, sent);
            if (attempt < (int)(sizeof(backoff) / sizeof(backoff[0]))) {
#if CONFIG_IDF_TARGET_LINUX
                usleep(backoff[attempt++] * 1000);  // Convert ticks to microseconds
#else
                vTaskDelay(backoff[attempt++]);
#endif
            } else {
#if CONFIG_IDF_TARGET_LINUX
                usleep(32 * 1000);
#else
                vTaskDelay(32);
#endif
            }
        }

        int64_t dt = esp_timer_get_time() - start;
        // A failed write aborts the connection and may return a short count (even 0) instead of -1
        if (sent == (int)len) {
            ESP_LOGI(TAG, "Echo success: opcode=0x%02X len=%d fin=%d in %lldus",
                     send_opcode, sent, last, (long long)dt);
        } else {
            ESP_LOGE(TAG, "Echo failed: opcode=0x%02X len=%d fin=%d",
                     send_opcode, (int)len, last);
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WebSocket error event");
        test_running = false;
        s_echo_in_progress = false;
        s_skip_frame = false;
        /* Same as DISCONNECTED: wait for FINISH before tearing down the client. */
        break;

    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WebSocket finish event");
        test_running = false;
        s_echo_in_progress = false;
        s_skip_frame = false;
#if CONFIG_IDF_TARGET_LINUX
        if (test_done_sem) {
            sem_post(test_done_sem);
        }
#else
        if (test_done_sem) {
            xSemaphoreGive(test_done_sem);
        }
#endif
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------
 *  Handler for get_case_count
 * ------------------------------------------------------------ */
static void get_case_count_event_handler(void *handler_args,
                                         esp_event_base_t base,
                                         int32_t event_id,
                                         void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_DATA:
        if (data->data_len > 0 && data->data_ptr) {
            // The server returns a JSON integer (e.g., "518")
            // We can parse it directly
            char *buf = (char *)malloc(data->data_len + 1);
            if (buf) {
                memcpy(buf, data->data_ptr, data->data_len);
                buf[data->data_len] = '\0';
                g_total_cases = atoi(buf);
                ESP_LOGI(TAG, "Received total case count: %d", g_total_cases);
                free(buf);
            }
        }
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------ */
static void get_case_count(void)
{
    char uri[512];
    int ret = snprintf(uri, sizeof(uri), "%s/getCaseCount", AUTOBAHN_SERVER_URI);
    if (ret < 0 || ret >= (int)sizeof(uri)) {
        ESP_LOGE(TAG, "URI too long");
        return;
    }

    ESP_LOGI(TAG, "Getting case count from: %s", uri);

    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .network_timeout_ms = 10000,
    };

    esp_websocket_client_handle_t client = esp_websocket_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init client for getCaseCount");
        return;
    }

    esp_websocket_register_events(client, WEBSOCKET_EVENT_DATA, get_case_count_event_handler, NULL);

    if (esp_websocket_client_start(client) == ESP_OK) {
        // Wait briefly for response
#if CONFIG_IDF_TARGET_LINUX
        usleep(2000 * 1000);
#else
        vTaskDelay(pdMS_TO_TICKS(2000));
#endif
        esp_websocket_client_stop(client);
    }
    esp_websocket_client_destroy(client);
}

/* ------------------------------------------------------------ */
/* Runs one connection to the Autobahn server until the server closes it (or 60 s pass) */
static esp_err_t run_client(const char *uri)
{
    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .buffer_size = BUFFER_SIZE,
        .network_timeout_ms = 10000,   // 10s for connection (default), 200ms was too short
        .disable_auto_reconnect = true, // Autobahn cases expect a clean stop on protocol errors
        .task_prio = 10,                // High prio → low latency
        .task_stack = 8144,
    };

    esp_websocket_client_handle_t client = esp_websocket_client_init(&cfg);
    if (!client) {
        return ESP_FAIL;
    }

    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY,
                                  websocket_event_handler, (void*)client);

#if CONFIG_IDF_TARGET_LINUX
    sem_init(&test_done_sem_storage, 0, 0);
    test_done_sem = &test_done_sem_storage;
#else
    test_done_sem = xSemaphoreCreateBinary();
#endif

    esp_err_t start_ret = esp_websocket_client_start(client);
    if (start_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_websocket_client_start() failed: err=0x%x", start_ret);
#if CONFIG_IDF_TARGET_LINUX
        if (test_done_sem) {
            sem_destroy(test_done_sem);
        }
#else
        if (test_done_sem) {
            vSemaphoreDelete(test_done_sem);
        }
#endif
        test_done_sem = NULL;
        esp_websocket_client_destroy(client);
        return start_ret;
    }

    /* Wait up to 60 s for FINISH (client task exiting). */
#if CONFIG_IDF_TARGET_LINUX
    int wait_ret;
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 60;  // absolute timeout (now + 60s)
        wait_ret = sem_timedwait(test_done_sem, &ts);
    }
    if (wait_ret != 0) {
        /* Timed out without FINISH — force-stop a still-running task. */
        esp_websocket_client_stop(client);
    } else {
        /* FINISH was posted just before transport_close + STOPPED_BIT.
         * Calling stop() here races that teardown and can hang on Linux.
         * Brief yield so destroy() sees STOPPED_BIT already set. */
        usleep(100 * 1000);
    }
#else
    if (xSemaphoreTake(test_done_sem, pdMS_TO_TICKS(60000)) != pdTRUE) {
        esp_websocket_client_stop(client);
    } else {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#endif

    esp_websocket_client_destroy(client);
#if CONFIG_IDF_TARGET_LINUX
    if (test_done_sem) {
        sem_destroy(test_done_sem);
    }
#else
    if (test_done_sem) {
        vSemaphoreDelete(test_done_sem);
    }
#endif
    test_done_sem = NULL;
    ESP_LOGI(TAG, "Free heap: %lu", esp_get_free_heap_size());
    return ESP_OK;
}

static esp_err_t run_test_case(int case_num)
{
    char uri[512];  // Increased to accommodate full URI + path
    int ret = snprintf(uri, sizeof(uri),
                       "%s/runCase?case=%d&agent=esp_websocket_client",
                       AUTOBAHN_SERVER_URI, case_num);
    if (ret < 0 || ret >= (int)sizeof(uri)) {
        ESP_LOGE(TAG, "URI too long: %s/runCase?case=%d&agent=esp_websocket_client", AUTOBAHN_SERVER_URI, case_num);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Running case %d: %s", case_num, uri);
    return run_client(uri);
}

/* ------------------------------------------------------------ */
static void update_reports(void)
{
    char uri[512];  // Increased to accommodate full URI + path
    int ret = snprintf(uri, sizeof(uri),
                       "%s/updateReports?agent=esp_websocket_client",
                       AUTOBAHN_SERVER_URI);
    if (ret < 0 || ret >= (int)sizeof(uri)) {
        ESP_LOGE(TAG, "URI too long: %s/updateReports?agent=esp_websocket_client", AUTOBAHN_SERVER_URI);
        return;
    }
    // The server writes the reports, then closes the connection; the echo handler only sees that CLOSE
    ESP_LOGI(TAG, "Updating reports: %s", uri);
    if (run_client(uri) == ESP_OK) {
        ESP_LOGI(TAG, "Reports updated");
    }
}

/* ------------------------------------------------------------ */
static void websocket_app_start(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, " Autobahn WebSocket Testsuite Client");
    ESP_LOGI(TAG, "====================================");

    ESP_LOGI(TAG, "Server: %s", AUTOBAHN_SERVER_URI);

    // Accumulator buffer is allocated on-demand only when fragmentation is detected.
    // This keeps memory usage low on constrained targets like ESP32-S2.

    // Attempt to fetch case count dynamically
    get_case_count();

    int start_case = DEFAULT_START_CASE;
    int end_case = g_total_cases > 0 ? g_total_cases : DEFAULT_END_CASE;

    ESP_LOGI(TAG, "Running tests from case %d to %d", start_case, end_case);

    for (int i = start_case; i <= end_case; i++) {
        ESP_LOGI(TAG, "========== Case %d/%d ==========", i, end_case);
        ESP_LOGI(TAG, "Starting test case %d...", i);
        esp_err_t ret = run_test_case(i);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Test case %d failed with error: 0x%x", i, ret);
        } else {
            ESP_LOGI(TAG, "Test case %d completed", i);
        }
#if CONFIG_IDF_TARGET_LINUX
        usleep(500 * 1000);  // 500ms
#else
        vTaskDelay(pdMS_TO_TICKS(500));
#endif
    }
    update_reports();

    ESP_LOGI(TAG, "All tests completed.");
}

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
/* ------------------------------------------------------------
 *  Read URI from stdin (similar to websocket_example.c)
 * ------------------------------------------------------------ */
static void get_string(char *line, size_t size)
{
    int count = 0;
    while (count < size - 1) {
        int c = fgetc(stdin);
        if (c == '\n' || c == '\r') {
            line[count] = '\0';
            break;
        } else if (c > 0 && c < 127) {
            line[count] = c;
            ++count;
        } else if (c == EOF) {
#if CONFIG_IDF_TARGET_LINUX
            usleep(10 * 1000);  // 10ms
#else
            vTaskDelay(pdMS_TO_TICKS(10));
#endif
        }
    }
    line[count] = '\0';
}
#endif /* CONFIG_WEBSOCKET_URI_FROM_STDIN */

/* ------------------------------------------------------------ */
#if CONFIG_IDF_TARGET_LINUX
int main(void)
#else
void app_main(void)
#endif
{
    // Disable stdout buffering for immediate output
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    ESP_LOGI(TAG, "Startup, IDF %s", esp_get_idf_version());
#if !CONFIG_IDF_TARGET_LINUX
    ESP_ERROR_CHECK(nvs_flash_init());
#endif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Accumulator buffer is allocated on-demand when needed (fragmented payloads).
    // Pre-allocating ~64KB here can prevent esp_websocket_client_init() from
    // allocating its own buffers on ESP32-S2.

    ESP_ERROR_CHECK(example_connect());

#if !CONFIG_IDF_TARGET_LINUX
    /* disable power‑save for low latency */
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
    // Read server URI from stdin
    ESP_LOGI(TAG, "Waiting for Autobahn server URI from stdin...");
    ESP_LOGI(TAG, "Please send URI in format: ws://<IP>:9001");
    // Loop until we get a valid URI
    do {
        get_string(g_autobahn_server_uri, sizeof(g_autobahn_server_uri));
        // Ensure null termination
        g_autobahn_server_uri[sizeof(g_autobahn_server_uri) - 1] = '\0';
    } while (strlen(g_autobahn_server_uri) == 0);

    ESP_LOGI(TAG, "Received server URI: %s", g_autobahn_server_uri);
#endif

    websocket_app_start();
#if CONFIG_IDF_TARGET_LINUX
    return 0;
#endif
}
