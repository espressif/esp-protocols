/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ESP libwebsockets client example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_task_wdt.h"

#include <cJSON.h>

static uint32_t disconnect_timeout = 5000000; // microseconds
static uint8_t message_count = 0;
static const char *TAG = "lws-client";

static struct lws_context *context;
static struct lws *client_wsi;

static lws_sorted_usec_list_t sul;
static unsigned char msg[LWS_PRE + 128];

static const lws_retry_bo_t retry = {
    .secs_since_valid_ping = 3,
    .secs_since_valid_hangup = 10,
};

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
static void get_string(char *line, size_t size)
{
    int count = 0;
    while (count < size) {
        int c = fgetc(stdin);
        if (c == '\n') {
            line[count] = '\0';
            break;
        } else if (c > 0 && c < 127) {
            line[count] = c;
            ++count;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }
}

#endif /* CONFIG_WEBSOCKET_URI_FROM_STDIN */

static void send_data(struct lws *wsi, const char *data, size_t len, enum lws_write_protocol type)
{
    unsigned char buf[LWS_PRE + len];
    unsigned char *p = &buf[LWS_PRE];
    memcpy(p, data, len);

    int n = lws_write(wsi, p, len, type);
    if (n < len) {
        ESP_LOGE(TAG, "ERROR %d writing ws\n", n);
    }
}

static void send_large_text_data(struct lws *wsi)
{
    const int size = 2000;
    char *long_data = malloc(size);
    memset(long_data, 'a', size);
    send_data(wsi, long_data, size, LWS_WRITE_TEXT);
    free(long_data);
}

static void send_fragmented_text_data(struct lws *wsi)
{
    char data[32];
    memset(data, 'a', sizeof(data));
    send_data(wsi, data, sizeof(data), LWS_WRITE_TEXT | LWS_WRITE_NO_FIN);
    memset(data, 'b', sizeof(data));
    send_data(wsi, data, sizeof(data), LWS_WRITE_CONTINUATION);
}

static void send_fragmented_binary_data(struct lws *wsi)
{
    char binary_data[5];
    memset(binary_data, 0, sizeof(binary_data));
    send_data(wsi, binary_data, sizeof(binary_data), LWS_WRITE_BINARY | LWS_WRITE_NO_FIN);
    memset(binary_data, 1, sizeof(binary_data));
    send_data(wsi, binary_data, sizeof(binary_data), LWS_WRITE_CONTINUATION);
}

static void connect_cb(lws_sorted_usec_list_t *_sul)
{
    struct lws_client_connect_info connect_info;

    ESP_LOGI(TAG, "%s: connecting\n", __func__);

    memset(&connect_info, 0, sizeof(connect_info));
#if CONFIG_WEBSOCKET_URI_FROM_STDIN
    char line[128];

    ESP_LOGI(TAG, "Please enter uri of websocket endpoint");
    get_string(line, sizeof(line));

    connect_info.address = line;
    ESP_LOGI(TAG, "Endpoint uri: %s\n", line);

#else
    connect_info.address = CONFIG_WEBSOCKET_URI;
#endif /* CONFIG_WEBSOCKET_URI_FROM_STDIN */


    connect_info.context = context;
    connect_info.port = CONFIG_WEBSOCKET_PORT;
    connect_info.host = connect_info.address;
    connect_info.origin = connect_info.address;
    connect_info.local_protocol_name = "lws-echo";
    connect_info.pwsi = &client_wsi;
    connect_info.retry_and_idle_policy = &retry;

#if defined(CONFIG_WS_OVER_TLS_MUTUAL_AUTH) || defined(CONFIG_WS_OVER_TLS_SERVER_AUTH)
    connect_info.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED;

#if defined(CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK) && defined(CONFIG_WS_OVER_TLS_SERVER_AUTH)
    connect_info.ssl_connection |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
#endif
#else
    connect_info.ssl_connection = LCCSCF_ALLOW_INSECURE;
#endif

    if (!lws_client_connect_via_info(&connect_info)) {
        lws_sul_schedule(context, 0, _sul, connect_cb, 5 * LWS_USEC_PER_SEC);
    }
}

static int callback_minimal_echo(struct lws *wsi, enum lws_callback_reasons reason,
                                 void *user, void *in, size_t len)
{
    switch (reason) {

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        ESP_LOGE(TAG, "CLIENT_CONNECTION_ERROR: %s\n",
                 in ? (char *)in : "(null)");
        lws_sul_schedule(context, 0, &sul, connect_cb, 5 * LWS_USEC_PER_SEC);
        break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        lws_callback_on_writable(wsi);

        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        if (message_count < 5) {
            char text_data[32];
            sprintf(text_data, "hello %04d", message_count++);
            ESP_LOGI(TAG, "Sending text: %s", text_data);
            send_data(wsi, text_data, strlen(text_data), LWS_WRITE_TEXT);
        } else if (message_count == 5) {
            ESP_LOGI(TAG, "Sending fragmented text message");
            send_fragmented_text_data(wsi);
            ESP_LOGI(TAG, "Sending fragmented binary message");
            send_fragmented_binary_data(wsi);
            ESP_LOGI(TAG, "Sending text longer than ws buffer (1024)");
            send_large_text_data(wsi);
            message_count++;
        }
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");

        if (lws_frame_is_binary(wsi)) {
            ESP_LOGI(TAG, "Received binary data");
            ESP_LOG_BUFFER_HEX("Received binary data", in, len);
        } else {
            ESP_LOGW(TAG, "Received=%.*s\n\n", len, (char *)in);
        }

        size_t remain = lws_remaining_packet_payload(wsi);

        // If received data is larger than the ws buffer
        if (remain > 0) {
            ESP_LOGW(TAG, "Total payload length=%u, data_len=%u\n\n", remain + len, len);
        }

        // If received data contains json structure it succeed to parse
        cJSON *root = cJSON_Parse(in);
        if (root) {
            for (int i = 0 ; i < cJSON_GetArraySize(root) ; i++) {
                cJSON *elem = cJSON_GetArrayItem(root, i);
                cJSON *id = cJSON_GetObjectItem(elem, "id");
                cJSON *name = cJSON_GetObjectItem(elem, "name");
                ESP_LOGW(TAG, "Json={'id': '%s', 'name': '%s'}", id->valuestring, name->valuestring);
            }
            cJSON_Delete(root);
        }

        /* Reset the timeout*/
        lws_set_timer_usecs(wsi, disconnect_timeout);
        break;
    case LWS_CALLBACK_TIMER:
        ESP_LOGW(TAG, "Closing connection");
        lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)"bye", 3);
        /* Return non-null to close the connection. */
        return -1;
        break;
    default:
        break;
    }

    return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
    {
        .name = "lws-echo",
        .callback = callback_minimal_echo,
        .per_session_data_size = 1024,
        .rx_buffer_size = 1024,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    LWS_PROTOCOL_LIST_TERM
};

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Configure WDT. */
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    esp_task_wdt_add(handle);

    /* Create LWS Context - Client. */
    struct lws_context_creation_info info;
    int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    memset(msg, 'x', sizeof(msg));

    lws_set_log_level(logs, NULL);
    ESP_LOGI(TAG, "LWS minimal ws client echo\n");

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
    info.protocols = protocols;
    info.fd_limit_per_thread = 1 + 1 + 1;

#if CONFIG_WS_OVER_TLS_MUTUAL_AUTH
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    /* Configuring client certificates for mutual authentification */
    extern const char cert_start[] asm("_binary_client_cert_pem_start"); // Client certificate
    extern const char cert_end[]   asm("_binary_client_cert_pem_end");
    extern const char key_start[] asm("_binary_client_key_pem_start"); // Client private key
    extern const char key_end[]   asm("_binary_client_key_pem_end");

    info.client_ssl_cert_mem        = cert_start;
    info.client_ssl_cert_mem_len    = cert_end - cert_start;
    info.client_ssl_key_mem         = key_start;
    info.client_ssl_key_mem_len     = key_end - key_start;
#elif CONFIG_WS_OVER_TLS_SERVER_AUTH
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    extern const char cacert_start[] asm("_binary_ca_cert_pem_start"); // CA certificate
    extern const char cacert_end[] asm("_binary_ca_cert_pem_end");

    info.client_ssl_ca_mem          = cacert_start;
    info.client_ssl_ca_mem_len      = cacert_end - cacert_start;
#endif

    context = lws_create_context(&info);
    if (!context) {
        ESP_LOGE(TAG, "lws init failed");
    } else {
        lws_sul_schedule(context, 0, &sul, connect_cb, 100);
        /*
        * Holds the result of the lws_service call:
        *  = 0 -> service succeeded and events were processed,
        *  < 0 -> an error occurred or the event loop should stop
        */
        int service_result = 0;
        while (service_result >= 0) {
            service_result = lws_service(context, 0);
        }

        lws_context_destroy(context);
    }

    while (1) {
        //Should not get here, Spin undefinitely.
        vTaskDelay(10);
        taskYIELD();
    }
}
