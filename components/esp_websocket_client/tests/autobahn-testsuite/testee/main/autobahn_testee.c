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
// Perf mode uses the target buffer size on Linux too, so it takes the same chunked path.
#if CONFIG_IDF_TARGET_LINUX && !CONFIG_AUTOBAHN_PERF_MODE
#define BUFFER_SIZE         65536
#else
#define BUFFER_SIZE         4096
#endif
// Configure test range here (case numbers depend on the fuzzing server config):
// Category 1 (Framing):          Tests 1-16
// Category 2 (Ping/Pong):        Tests 17-27
// Category 3 (Reserved Bits):    Tests 28-34
// Category 4 (Opcodes):          Tests 35-44
// Category 5 (Fragmentation):    Tests 45-64
// Category 6 (UTF-8):            Tests 65-209
// Category 7 (Close Handshake):  Tests 210-246
// Category 9 (Performance):      9.* only — use config/fuzzingserver-perf.json
//                                and CONFIG_AUTOBAHN_PERF_MODE
// Category 10 (Miscellaneous):   After 7 when 9.* is excluded (e.g. 10.1.1)
// All tests:                     Tests 1-300
// Defaults if get_case_count fails
#define DEFAULT_START_CASE  1
#define DEFAULT_END_CASE    300
#if CONFIG_AUTOBAHN_PERF_MODE
#define PERF_MODE           1
#define TEST_CASE_WAIT_SEC  1010  // Just above the server's longest case limit (1000s, 9.5.1)
#else
#define PERF_MODE           0
#define TEST_CASE_WAIT_SEC  60
#endif

#if CONFIG_IDF_TARGET_LINUX
static sem_t test_done_sem_storage;
static sem_t *test_done_sem = NULL;
#else
static SemaphoreHandle_t test_done_sem = NULL;
#endif
static bool test_running = false;
static volatile bool s_case_aborted = false;  // Set by the client task, read by run_test_case()

// Global to store total case count fetched from server
static int g_total_cases = 0;

#define MAX_ACCUMULATED_PAYLOAD 65537  // Largest compliance message is 64KB+1 (case 1.1.6)

/* Compliance mode reassembles a message and echoes it with one send call, which
 * also exercises the client's splitting of payloads larger than buffer_size.
 */
typedef struct {
    uint8_t *buffer;
    size_t   capacity;
    size_t   received;
    uint8_t  opcode;
    bool     active;
} ws_accumulator_t;

static ws_accumulator_t s_accumulator = {0};

static void ws_accumulator_reset(void)
{
    // Reset state but keep buffer allocated for reuse
    s_accumulator.received = 0;
    s_accumulator.opcode   = 0;
    s_accumulator.active   = false;
}

static void ws_accumulator_cleanup(void)
{
    free(s_accumulator.buffer);
    s_accumulator.buffer = NULL;
    s_accumulator.capacity = 0;
    ws_accumulator_reset();
}

/* frame_remaining includes this chunk, so one allocation covers an unfragmented frame. */
static esp_err_t ws_accumulator_append(uint8_t opcode, const uint8_t *data, size_t len, size_t frame_remaining)
{
    size_t reserve = s_accumulator.received + ((frame_remaining > len) ? frame_remaining : len);

    if (reserve > MAX_ACCUMULATED_PAYLOAD) {
        ESP_LOGE(TAG, "Message too large (%zu > %d); 9.* needs CONFIG_AUTOBAHN_PERF_MODE",
                 reserve, MAX_ACCUMULATED_PAYLOAD);
        return ESP_ERR_INVALID_SIZE;
    }
    if (reserve > s_accumulator.capacity) {
        uint8_t *new_ptr = (uint8_t *)realloc(s_accumulator.buffer, reserve);
        if (!new_ptr) {
            ESP_LOGE(TAG, "Accumulator alloc failed (%zu bytes), free heap: %lu",
                     reserve, (unsigned long)esp_get_free_heap_size());
            return ESP_ERR_NO_MEM;
        }
        s_accumulator.buffer = new_ptr;
        s_accumulator.capacity = reserve;
    }
    if (!s_accumulator.active) {
        s_accumulator.opcode = opcode;
        s_accumulator.active = true;
    }
    if (len > 0) {
        memcpy(s_accumulator.buffer + s_accumulator.received, data, len);
        s_accumulator.received += len;
    }
    return ESP_OK;
}

/* Called from the client task, which cannot stop itself: wake run_test_case() to stop it. */
static void abort_test_case(void)
{
    s_case_aborted = true;
    test_running = false;
    ws_accumulator_reset();
#if CONFIG_IDF_TARGET_LINUX
    if (test_done_sem) {
        sem_post(test_done_sem);
    }
#else
    if (test_done_sem) {
        xSemaphoreGive(test_done_sem);
    }
#endif
}

static TickType_t echo_timeout_ticks(size_t len)
{
    /* WiFi TCP windows fill during Autobahn 9.* (multi-MB echo). poll_write
     * must wait for the window to open — 10ms on a 224-byte chunk aborts the
     * connection (errno 119) and leaves recv stuck on the remainder of the frame.
     */
#if CONFIG_IDF_TARGET_LINUX
    int send_timeout_ms = (len > 1024) ? 2000 : 500;
#else
    int send_timeout_ms = 5000;
    (void)len;
#endif
    return pdMS_TO_TICKS(send_timeout_ms);
}

/* Echo one WS frame, preserving FIN. send_with_opcode always sets FIN, which is
 * also how a final CONT frame is sent; FIN=0 frames need the partial/cont APIs.
 */
static int echo_ws_frame(esp_websocket_client_handle_t client, uint8_t opcode,
                         const uint8_t *payload, size_t len, bool fin, TickType_t timeout)
{
    const char *data = (len > 0) ? (const char *)payload : NULL;
    int slen = (int)len;

    if (fin) {
        return esp_websocket_client_send_with_opcode(client, opcode, (const uint8_t *)data, slen, timeout);
    }
    if (opcode == WS_TRANSPORT_OPCODES_TEXT) {
        return esp_websocket_client_send_text_partial(client, data, slen, timeout);
    }
    if (opcode == WS_TRANSPORT_OPCODES_BINARY) {
        return esp_websocket_client_send_bin_partial(client, data, slen, timeout);
    }
    return esp_websocket_client_send_cont_msg(client, data, slen, timeout);
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
        ws_accumulator_reset();
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        test_running = false;
        ws_accumulator_reset();
        /* Do not signal test_done here: ERROR/DISCONNECTED can fire while the
         * client task still holds its lock mid-abort. Wait for FINISH so stop/destroy
         * does not race the task teardown (hangs on Linux host). */
        break;

    case WEBSOCKET_EVENT_DATA: {
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_DATA: opcode=0x%02X len=%d fin=%d payload_len=%d offset=%d",
                 data->op_code, data->data_len, data->fin, data->payload_len, data->payload_offset);

        // Safety check: if not connected, don't process data
        if (!test_running || !esp_websocket_client_is_connected(client)) {
            ESP_LOGW(TAG, "Received data but not connected, ignoring");
            ws_accumulator_reset();
            break;
        }

        /* ---- skip control frames ---- */
        if (data->op_code >= 0x08) {
            if (data->op_code == 0x09) {
                ESP_LOGD(TAG, "PING -> PONG auto-sent");
            }
            break;
        }

        /* ---- Determine opcode to echo ---- */
        uint8_t send_opcode = 0;
        if (data->op_code == 0x1) {
            send_opcode = WS_TRANSPORT_OPCODES_TEXT;
        } else if (data->op_code == 0x2) {
            send_opcode = WS_TRANSPORT_OPCODES_BINARY;
        } else if (data->op_code == 0x0) {
            send_opcode = WS_TRANSPORT_OPCODES_CONT;
        } else {
            ESP_LOGW(TAG, "Unsupported opcode 0x%02X - skip", data->op_code);
            break;
        }

        // Safety check: validate data pointer before processing
        if (!data->data_ptr && data->data_len > 0) {
            ESP_LOGE(TAG, "NULL data pointer with non-zero length: %d", data->data_len);
            break;
        }

        const uint8_t *payload = (const uint8_t *)data->data_ptr;
        size_t len = data->data_len;

        /* A read timeout mid-frame arrives as len=0 with payload_len still set. The
         * transport has then dropped its frame state and would parse the remaining
         * payload as a header, so the stream cannot be trusted. A real empty WS frame
         * has payload_len == 0.
         */
        if (data->data_len == 0 && data->payload_len > 0) {
            ESP_LOGE(TAG, "Read timeout inside a frame (offset=%d payload_len=%d), aborting case",
                     data->payload_offset, data->payload_len);
            abort_test_case();
            break;
        }

        /* A single WS frame is posted as multiple EVENT_DATA callbacks when
         * payload_len > buffer_size. Later chunks still report the original opcode.
         */
        bool ws_frame_complete = (data->payload_len == 0) ||
                                 ((size_t)data->payload_offset + (size_t)data->data_len >= (size_t)data->payload_len);
        bool echo_fin = data->fin && ws_frame_complete;
        bool used_accumulator = false;

        ESP_LOGD(TAG, "Frame check: offset=%d payload_len=%d data_len=%d fin=%d complete=%d echo_fin=%d",
                 data->payload_offset, data->payload_len, data->data_len, data->fin, ws_frame_complete, echo_fin);

        if (PERF_MODE) {
            /* Echo each chunk immediately so multi-MB messages need no buffering. */
            if (data->payload_offset > 0) {
                send_opcode = WS_TRANSPORT_OPCODES_CONT;
            }
        } else if (s_accumulator.active || !echo_fin || data->payload_offset > 0) {
            if (!s_accumulator.active && send_opcode == WS_TRANSPORT_OPCODES_CONT) {
                ESP_LOGW(TAG, "Continuation frame without a started message, skipping");
                break;
            }
            size_t frame_remaining = (data->payload_len > data->payload_offset) ?
                                     (size_t)(data->payload_len - data->payload_offset) : len;
            if (ws_accumulator_append(send_opcode, payload, len, frame_remaining) != ESP_OK) {
                abort_test_case();
                break;
            }
            if (!echo_fin) {
                break;  // Wait for the rest of the message
            }
            payload          = s_accumulator.buffer;
            len              = s_accumulator.received;
            send_opcode      = s_accumulator.opcode;
            used_accumulator = true;
        }

        // Check connection before attempting to send
        if (!esp_websocket_client_is_connected(client)) {
            ESP_LOGW(TAG, "Connection lost before echo, skipping");
            ws_accumulator_reset();
            break;
        }

        int sent = -1;
        int attempt = 0;
        const TickType_t backoff[] = {1, 1, 1, 2, 4, 8};  // Shorter backoff for faster retry
        int64_t start = esp_timer_get_time();
        TickType_t send_timeout = echo_timeout_ticks(len);

        while (sent < 0 && esp_websocket_client_is_connected(client)) {
            ESP_LOGD(TAG, "Sending echo: opcode=0x%02X len=%zu fin=%d",
                     send_opcode, len, echo_fin);

            sent = echo_ws_frame(client, send_opcode, payload, len, echo_fin, send_timeout);

            /* send_raw returns 0 on poll_write timeout and aborts the connection.
             * Do not treat that as success when we still had payload to send.
             */
            if (sent > 0 || (sent == 0 && len == 0)) {
                ESP_LOGD(TAG, "Echo sent successfully: %d bytes", sent);
                break;
            }
            if (sent == 0) {
                sent = -1;
            }
            ESP_LOGW(TAG,
                     "echo send retry: opcode=0x%02X len=%zu fin=%d attempt=%d sent=%d",
                     send_opcode, len, echo_fin, attempt + 1, sent);
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
        if (sent >= 0) {
            ESP_LOGD(TAG, "Echo success: opcode=0x%02X len=%d fin=%d in %lldus",
                     data->op_code, sent, echo_fin, (long long)dt);
        } else {
            ESP_LOGE(TAG, "Echo failed: opcode=0x%02X len=%d fin=%d",
                     data->op_code, (int)len, echo_fin);
        }

        /* Free the message buffer now to avoid holding it across cases (helps ESP32-S2). */
        if (used_accumulator) {
            ws_accumulator_cleanup();
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WebSocket error event");
        test_running = false;
        ws_accumulator_reset();  // Reset accumulator on error
        /* Same as DISCONNECTED: wait for FINISH before tearing down the client. */
        break;

    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WebSocket finish event");
        test_running = false;
        ws_accumulator_reset();  // Reset accumulator on finish
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
    ws_accumulator_reset();
    s_case_aborted = false;

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

    /* Wait for FINISH (client task exiting) or abort_test_case(). */
#if CONFIG_IDF_TARGET_LINUX
    int wait_ret;
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += TEST_CASE_WAIT_SEC;
        wait_ret = sem_timedwait(test_done_sem, &ts);
    }
    if (wait_ret != 0 || s_case_aborted) {
        /* No FINISH yet — force-stop a still-running task. */
        esp_websocket_client_stop(client);
    } else {
        /* FINISH was posted just before transport_close + STOPPED_BIT.
         * Calling stop() here races that teardown and can hang on Linux.
         * Brief yield so destroy() sees STOPPED_BIT already set. */
        usleep(100 * 1000);
    }
#else
    if (xSemaphoreTake(test_done_sem, pdMS_TO_TICKS(TEST_CASE_WAIT_SEC * 1000)) != pdTRUE || s_case_aborted) {
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
    esp_websocket_client_config_t cfg = { .uri = uri };
    esp_websocket_client_handle_t client = esp_websocket_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client for update_reports");
        return;
    }
    esp_err_t start_ret = esp_websocket_client_start(client);
    if (start_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_websocket_client_start() failed for update_reports: err=0x%x", start_ret);
        esp_websocket_client_destroy(client);
        return;
    }
#if CONFIG_IDF_TARGET_LINUX
    usleep(3000 * 1000);  // 3 seconds
#else
    vTaskDelay(pdMS_TO_TICKS(3000));
#endif
    esp_websocket_client_stop(client);
    esp_websocket_client_destroy(client);
    ESP_LOGI(TAG, "Reports updated");
}

/* ------------------------------------------------------------ */
static void websocket_app_start(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, " Autobahn WebSocket Testsuite Client");
    ESP_LOGI(TAG, "====================================");

    ESP_LOGI(TAG, "Server: %s", AUTOBAHN_SERVER_URI);

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

    ws_accumulator_cleanup();
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
