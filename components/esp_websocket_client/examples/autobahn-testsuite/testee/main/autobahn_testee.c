/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

#if CONFIG_IDF_TARGET_LINUX
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
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
#define BUFFER_SIZE         16384  // Reduced from 32768 to free memory for accumulator buffer
#define START_CASE          1
#define END_CASE            300
// Configure test range here:
// Category 1 (Framing):          Tests 1-16
// Category 2 (Ping/Pong):        Tests 17-27
// Category 3 (Reserved Bits):    Tests 28-34
// Category 4 (Opcodes):          Tests 35-44
// Category 5 (Fragmentation):    Tests 45-64
// Category 6 (UTF-8):            Tests 65-209
// Category 7 (Close Handshake):  Tests 210-246
// All tests:                     Tests 1-300
#if CONFIG_IDF_TARGET_LINUX
static sem_t test_done_sem_storage;
static sem_t *test_done_sem = NULL;
#else
static SemaphoreHandle_t test_done_sem = NULL;
#endif
static bool test_running = false;

#define MAX_FRAGMENTED_PAYLOAD 65537  // Maximum payload size for fragmented frames (case 1.1.6=65535, 1.1.7=65536)

typedef struct {
    uint8_t *buffer;
    size_t   capacity;
    size_t   expected_len;
    size_t   received;
    uint8_t  opcode;
    bool     active;
} ws_accumulator_t;

static ws_accumulator_t s_accumulator = {0};
static uint8_t *s_accum_buffer = NULL;  // Pre-allocated buffer for fragmented frames

static void ws_accumulator_reset(void)
{
    // Reset state but keep buffer allocated for reuse
    s_accumulator.expected_len = 0;
    s_accumulator.received     = 0;
    s_accumulator.opcode       = 0;
    s_accumulator.active       = false;
}

static void ws_accumulator_cleanup(void)
{
    ws_accumulator_reset();
    if (s_accum_buffer) {
        free(s_accum_buffer);
        s_accum_buffer = NULL;
        ESP_LOGD(TAG, "Freed accumulator buffer");
    }
}

static esp_err_t ws_accumulator_prepare(size_t total_len, uint8_t opcode)
{
    if (total_len == 0) {
        return ESP_OK;
    }

    if (total_len > MAX_FRAGMENTED_PAYLOAD) {
        ESP_LOGE(TAG, "Payload too large (%zu > %d)", total_len, MAX_FRAGMENTED_PAYLOAD);
        return ESP_ERR_INVALID_SIZE;
    }

    // Allocate buffer on-demand when first fragmented frame is detected
    // This avoids allocating 64KB upfront which can cause memory exhaustion
    if (!s_accum_buffer) {
        size_t free_heap = esp_get_free_heap_size();
#if CONFIG_IDF_TARGET_LINUX
        size_t largest_free = free_heap;  // Linux has plenty of memory
#else
        size_t largest_free = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
#endif
        ESP_LOGD(TAG, "Attempting accumulator alloc: need=%zu, free=%zu, largest_block=%zu",
                 MAX_FRAGMENTED_PAYLOAD, free_heap, largest_free);

        s_accum_buffer = (uint8_t *)malloc(MAX_FRAGMENTED_PAYLOAD);
        if (!s_accum_buffer) {
            ESP_LOGE(TAG, "Accumulator alloc failed (%zu bytes) - Free heap: %zu, largest block: %zu",
                     total_len, free_heap, largest_free);
#if !CONFIG_IDF_TARGET_LINUX
            ESP_LOGE(TAG, "ESP32-S2 may not have enough RAM. Consider reducing BUFFER_SIZE or using SPIRAM");
#endif
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGD(TAG, "Allocated accumulator buffer: %d bytes (Free heap: %lu)",
                 MAX_FRAGMENTED_PAYLOAD, esp_get_free_heap_size());
    }

    s_accumulator.buffer       = s_accum_buffer;
    s_accumulator.capacity     = MAX_FRAGMENTED_PAYLOAD;
    s_accumulator.expected_len = total_len;
    s_accumulator.received     = 0;
    s_accumulator.opcode       = opcode;
    s_accumulator.active       = true;
    return ESP_OK;
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
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        test_running = false;
        ws_accumulator_reset();  // Reset state but keep buffer for next test
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

    case WEBSOCKET_EVENT_DATA: {
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA: opcode=0x%02X len=%d fin=%d payload_len=%d offset=%d",
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

        /* Note: send_with_opcode always sets FIN bit, which is correct for these
         * simple test cases (all have FIN=1). For fragmented messages, we'd need
         * send_with_exact_opcode, but it's not public. */

        // Safety check: validate data pointer before processing
        if (!data->data_ptr && data->data_len > 0) {
            ESP_LOGE(TAG, "NULL data pointer with non-zero length: %d", data->data_len);
            break;
        }

        const uint8_t *payload = (const uint8_t *)data->data_ptr;
        size_t len = data->data_len;

        // Check if this is a fragmented message (either WebSocket fragmentation or TCP-level fragmentation)
        // The WebSocket layer reads large frames in chunks and dispatches multiple events:
        // - payload_len = total frame size (set on all chunks)
        // - payload_offset = current offset (0, buffer_size, 2*buffer_size, ...)
        // - data_len = current chunk size
        // - fin = 1 only on the last chunk
        // So fragmentation is detected if: payload_len > data_len OR payload_offset > 0
        size_t total_len = data->payload_len ? data->payload_len : data->data_len;
        bool fragmented = (data->payload_len > 0 && data->payload_len > data->data_len) ||
                          (data->payload_offset > 0);

        ESP_LOGD(TAG, "Fragmentation check: offset=%d payload_len=%d data_len=%d total_len=%zu fragmented=%d",
                 data->payload_offset, data->payload_len, data->data_len, total_len, fragmented);

        if (fragmented && total_len > 0) {
            // Additional safety: check if buffer pointer is valid
            if (!s_accum_buffer) {
                ESP_LOGE(TAG, "Accumulator buffer not allocated, attempting allocation...");
                s_accum_buffer = (uint8_t *)malloc(MAX_FRAGMENTED_PAYLOAD);
                if (!s_accum_buffer) {
                    ESP_LOGE(TAG, "Failed to allocate accumulator buffer, skipping fragmented message");
                    break;
                }
            }

            if (data->payload_offset == 0 || !s_accumulator.active) {
                if (ws_accumulator_prepare(total_len, send_opcode) != ESP_OK) {
                    ESP_LOGE(TAG, "Cannot allocate buffer for fragmented frame len=%zu", total_len);
                    break;
                }
            } else if (total_len != s_accumulator.expected_len) {
                ESP_LOGW(TAG, "Payload len changed mid-message (%zu -> %zu) - reset accumulator",
                         s_accumulator.expected_len, total_len);
                ws_accumulator_reset();
                if (ws_accumulator_prepare(total_len, send_opcode) != ESP_OK) {
                    break;
                }
            }

            if (!s_accumulator.active || !s_accumulator.buffer) {
                ESP_LOGE(TAG, "Accumulator inactive or buffer NULL while processing fragments");
                ws_accumulator_reset();
                break;
            }

            size_t offset = data->payload_offset;
            // Safety checks before memcpy
            if (!data->data_ptr && data->data_len > 0) {
                ESP_LOGE(TAG, "NULL data pointer with non-zero length: %d", data->data_len);
                ws_accumulator_reset();
                break;
            }
            if (offset + data->data_len > s_accumulator.capacity) {
                ESP_LOGE(TAG, "Accumulator overflow: off=%zu chunk=%d cap=%zu",
                         offset, data->data_len, s_accumulator.capacity);
                ws_accumulator_reset();
                break;
            }
            if (offset + data->data_len > s_accumulator.expected_len) {
                ESP_LOGE(TAG, "Data exceeds expected length: off=%zu chunk=%d expected=%zu",
                         offset, data->data_len, s_accumulator.expected_len);
                ws_accumulator_reset();
                break;
            }

            // Final safety check before memcpy
            if (!s_accumulator.buffer) {
                ESP_LOGE(TAG, "Buffer became NULL before memcpy!");
                ws_accumulator_reset();
                break;
            }

            memcpy(s_accumulator.buffer + offset, data->data_ptr, data->data_len);
            s_accumulator.received = offset + data->data_len;

            if (s_accumulator.received < s_accumulator.expected_len) {
                // wait for more fragments
                ESP_LOGD(TAG, "Waiting for more fragments: received=%zu expected=%zu",
                         s_accumulator.received, s_accumulator.expected_len);
                break;
            }

            // Completed full message
            payload     = s_accumulator.buffer;
            len         = s_accumulator.expected_len;
            send_opcode = s_accumulator.opcode;
            s_accumulator.active = false;
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

        /* Send echo immediately - use timeout scaled by frame size for large frames */
        /* For large messages (>16KB), the send function fragments into multiple chunks */
        /* Each chunk needs sufficient timeout, so scale timeout per chunk, not total message */
        while (sent < 0 && esp_websocket_client_is_connected(client)) {
            /* For zero-length payload, pass NULL pointer (API handles this correctly) */
            /* Calculate timeout per chunk: large messages are fragmented, each chunk needs time */
#if CONFIG_IDF_TARGET_LINUX
            int send_timeout_ms = 5;  // Default 5ms for small frames
            if (len > 1024) {
                send_timeout_ms = 500;  // 500ms per chunk for large messages
            } else {
                send_timeout_ms = (len / 256) + 10;
                if (send_timeout_ms > 100) {
                    send_timeout_ms = 100;
                }
            }
            TickType_t send_timeout = pdMS_TO_TICKS(send_timeout_ms);
            ESP_LOGD(TAG, "Sending echo: opcode=0x%02X len=%zu timeout=%dms",
                     send_opcode, len, send_timeout_ms);
#else
            TickType_t send_timeout = pdMS_TO_TICKS(5);  // Default 5ms for small frames
            if (len > 1024) {
                // For large messages, use a per-chunk timeout that accounts for network delays
                // Since messages are fragmented into ~16KB chunks, each chunk needs sufficient time
                // Use a fixed generous timeout per chunk for large messages (500ms per chunk)
                // For 65535 bytes = 4 chunks, total time could be up to 2 seconds
                send_timeout = pdMS_TO_TICKS(500);  // 500ms per chunk for large messages
            } else {
                // Small messages: scale timeout based on size
                send_timeout = pdMS_TO_TICKS((len / 256) + 10);
                if (send_timeout > pdMS_TO_TICKS(100)) {
                    send_timeout = pdMS_TO_TICKS(100);
                }
            }

            ESP_LOGD(TAG, "Sending echo: opcode=0x%02X len=%zu timeout=%lums",
                     send_opcode, len, (unsigned long)(send_timeout * portTICK_PERIOD_MS));
#endif

            sent = esp_websocket_client_send_with_opcode(
                       client, send_opcode,
                       (len > 0) ? payload : NULL, len,
                       send_timeout);

            if (sent >= 0) {
                ESP_LOGD(TAG, "Echo sent successfully: %d bytes", sent);
                break;
            }
            ESP_LOGW(TAG,
                     "echo send retry: opcode=0x%02X len=%zu fin=%d attempt=%d sent=%d",
                     send_opcode, len, data->fin, attempt + 1, sent);
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
            ESP_LOGI(TAG, "Echo success: opcode=0x%02X len=%d fin=%d in %lldus",
                     data->op_code, sent, data->fin, (long long)dt);
        } else {
            ESP_LOGE(TAG, "Echo failed: opcode=0x%02X len=%d fin=%d",
                     data->op_code, (int)len, data->fin);
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WebSocket error event");
        test_running = false;
        ws_accumulator_reset();  // Reset accumulator on error
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

    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGD(TAG, "WebSocket finish event");
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

    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .buffer_size = BUFFER_SIZE,
        .network_timeout_ms = 10000,   // 10s for connection (default), 200ms was too short
        .reconnect_timeout_ms = 500,
        .task_prio = 10,                // High prio → low latency
        .task_stack = 8144,
    };

    // If accumulator buffer is not allocated yet, try to allocate it now
    // (before client init to avoid fragmentation)
    if (!s_accum_buffer) {
        ESP_LOGD(TAG, "Attempting to allocate accumulator buffer before client init (Free heap: %lu)",
                 esp_get_free_heap_size());
        s_accum_buffer = (uint8_t *)malloc(MAX_FRAGMENTED_PAYLOAD);
        if (s_accum_buffer) {
            ESP_LOGD(TAG, "Successfully allocated accumulator buffer: %d bytes", MAX_FRAGMENTED_PAYLOAD);
        }
    }

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

    /* Wait up to 60 s so server can close properly */
#if CONFIG_IDF_TARGET_LINUX
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 60;  // absolute timeout (now + 60s)
        (void)sem_timedwait(test_done_sem, &ts);
    }
#else
    xSemaphoreTake(test_done_sem, pdMS_TO_TICKS(60000));
#endif

    if (esp_websocket_client_is_connected(client)) {
        esp_websocket_client_stop(client);
    }

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
    esp_websocket_client_start(client);
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
    printf("=== Inside websocket_app_start() ===\n");

    printf("About to log banner...\n");
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, " Autobahn WebSocket Testsuite Client");
    ESP_LOGI(TAG, "====================================");

    printf("About to log server URI...\n");
    printf("AUTOBAHN_SERVER_URI = %s\n", AUTOBAHN_SERVER_URI);
    ESP_LOGI(TAG, "Server: %s", AUTOBAHN_SERVER_URI);

    printf("About to check accumulator buffer...\n");
    // Accumulator buffer should already be allocated in app_main() before any clients
    // If not, it will be allocated on-demand when first fragmented frame is detected
    if (s_accum_buffer) {
        ESP_LOGI(TAG, "Accumulator buffer ready: %d bytes", MAX_FRAGMENTED_PAYLOAD);
    } else {
        ESP_LOGW(TAG, "Accumulator buffer not pre-allocated, will allocate on-demand (max %d bytes)", MAX_FRAGMENTED_PAYLOAD);
    }

    printf("About to start test loop...\n");
    for (int i = START_CASE; i <= END_CASE; i++) {
        ESP_LOGI(TAG, "========== Case %d/%d ==========", i, END_CASE);
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

    // Free accumulator buffer after all tests
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
        }
#if CONFIG_IDF_TARGET_LINUX
        usleep(10 * 1000);  // 10ms
#else
        vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
    }
    if (count == size - 1) {
        line[count] = '\0';
    }
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

    printf("=== Autobahn testee starting ===\n");
    ESP_LOGI(TAG, "Startup, IDF %s", esp_get_idf_version());
#if !CONFIG_IDF_TARGET_LINUX
    ESP_ERROR_CHECK(nvs_flash_init());
#endif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Allocate accumulator buffer early, before any WebSocket clients are created
    // This ensures we have enough contiguous memory before heap gets fragmented
    // ESP32-S2 has limited RAM (~320KB total), so we need to allocate this early
    printf("About to allocate accumulator buffer...\n");
    ESP_LOGI(TAG, "Allocating accumulator buffer early (Free heap: %lu)", esp_get_free_heap_size());
    s_accum_buffer = (uint8_t *)malloc(MAX_FRAGMENTED_PAYLOAD);
    printf("Accumulator buffer allocated: %p\n", (void*)s_accum_buffer);
    if (!s_accum_buffer) {
        ESP_LOGE(TAG, "Failed to allocate accumulator buffer (%d bytes) - Free heap: %lu",
                 MAX_FRAGMENTED_PAYLOAD, esp_get_free_heap_size());
        ESP_LOGE(TAG, "ESP32-S2 may not have enough RAM for 64KB buffer. Consider:");
        ESP_LOGE(TAG, "  1. Reducing BUFFER_SIZE further (currently %d)", BUFFER_SIZE);
        ESP_LOGE(TAG, "  2. Using SPIRAM if available");
        ESP_LOGE(TAG, "  3. Skipping large payload tests (case 1.1.6)");
        // Continue anyway - will try on-demand allocation later
    } else {
        ESP_LOGI(TAG, "Successfully allocated accumulator buffer: %d bytes (Free heap: %lu)",
                 MAX_FRAGMENTED_PAYLOAD, esp_get_free_heap_size());
    }

    printf("About to call example_connect()...\n");
    ESP_ERROR_CHECK(example_connect());
    printf("example_connect() completed\n");

#if !CONFIG_IDF_TARGET_LINUX
    /* disable power‑save for low latency */
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
    // Read server URI from stdin
    printf("About to read URI from stdin...\n");
    ESP_LOGI(TAG, "Waiting for Autobahn server URI from stdin...");
    ESP_LOGI(TAG, "Please send URI in format: ws://<IP>:9001");
    printf("Calling get_string()...\n");
    get_string(g_autobahn_server_uri, sizeof(g_autobahn_server_uri));
    // Ensure null termination
    g_autobahn_server_uri[sizeof(g_autobahn_server_uri) - 1] = '\0';
    printf("get_string() returned, URI: %s\n", g_autobahn_server_uri);

    printf("About to call ESP_LOGI...\n");
    // ESP_LOGI(TAG, "Received server URI: %s", g_autobahn_server_uri);
    printf("ESP_LOGI skipped, continuing...\n");

    printf("About to check strlen...\n");
    size_t uri_len = strlen(g_autobahn_server_uri);
    printf("strlen returned: %zu\n", uri_len);

    if (uri_len == 0) {
        printf("URI is empty, using default\n");
        ESP_LOGE(TAG, "No URI received from stdin, using default");
#ifdef CONFIG_AUTOBAHN_SERVER_URI
        strncpy(g_autobahn_server_uri, CONFIG_AUTOBAHN_SERVER_URI, sizeof(g_autobahn_server_uri) - 1);
#else
        strncpy(g_autobahn_server_uri, "ws://192.168.1.100:9001", sizeof(g_autobahn_server_uri) - 1);
#endif
    }
#endif

    printf("About to call websocket_app_start()...\n");
    websocket_app_start();
    printf("websocket_app_start() returned\n");
#if CONFIG_IDF_TARGET_LINUX
    return 0;
#endif
}
