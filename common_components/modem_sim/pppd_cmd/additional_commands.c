/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_at.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_timer.h"

static uint8_t at_test_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "test command: <AT%s=?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "query command: <AT%s?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cmd_test(uint8_t para_num)
{
    uint8_t index = 0;
    printf("setup command: <AT%s=%d> is executed\r\n", esp_at_get_current_cmd_name(), para_num);

    // get first parameter, and parse it into a digit
    int32_t digit = 0;
    if (esp_at_get_para_as_digit(index++, &digit) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    printf("digit: %d\r\n", digit);

    // get second parameter, and parse it into a string
    uint8_t *str = NULL;
    if (esp_at_get_para_as_str(index++, &str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    printf("string: %s\r\n", str);

    // allocate a buffer and construct the data, then send the data to mcu via interface (uart/spi/sdio/socket)
    uint8_t *buffer = (uint8_t *)malloc(512);
    if (!buffer) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    int len = snprintf((char *)buffer, 512, "setup command: <AT%s=%d,\"%s\"> is executed\r\n",
                       esp_at_get_current_cmd_name(), digit, str);
    esp_at_port_write_data(buffer, len);

    // remember to free the buffer
    free(buffer);

    return ESP_AT_RESULT_CODE_OK;
}

#define TAG "at_custom_cmd"
static esp_netif_t *s_netif = NULL;
static httpd_handle_t http_server = NULL;

static void on_ppp_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_netif_t **netif = data;
    if (base == NETIF_PPP_STATUS && event_id == NETIF_PPP_ERRORUSER) {
        printf("Disconnected!");
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    esp_netif_t *netif = event->esp_netif;
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        printf("Got IPv4 event: Interface \"%s(%s)\" address: " IPSTR, esp_netif_get_desc(netif),
               esp_netif_get_ifkey(netif), IP2STR(&event->ip_info.ip));
        ESP_ERROR_CHECK(esp_netif_napt_enable(s_netif));

    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Disconnected");
    }
}

static SemaphoreHandle_t at_sync_sema = NULL;
static void wait_data_callback(void)
{
    static uint8_t buffer[1500] = {0};
    int len = esp_at_port_read_data(buffer, sizeof(buffer) - 1);

    // Check for the escape sequence "+++" in the received data
    const uint8_t escape_seq[] = "+++";
    uint8_t *escape_ptr = memmem(buffer, len, escape_seq, 3);

    if (escape_ptr != NULL) {
        printf("Found +++ sequence, signal to the command processing thread\n");

        int data_before_escape = escape_ptr - buffer;
        if (data_before_escape > 0) {
            esp_netif_receive(s_netif, buffer, data_before_escape, NULL);
        }

        if (at_sync_sema) {
            xSemaphoreGive(at_sync_sema);
        }
        return;
    }
    esp_netif_receive(s_netif, buffer, len, NULL);
}

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    printf("transmit: %d bytes\n", len);
    esp_at_port_write_data(buffer, len);
    return ESP_OK;
}

static uint8_t at_exe_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "execute command: <AT%s> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    printf("Command <AT%s> executed successfully\r\n", cmd_name);
    if (!at_sync_sema) {
        at_sync_sema = xSemaphoreCreateBinary();
        assert(at_sync_sema != NULL);
        esp_netif_driver_ifconfig_t driver_cfg = {
            .handle = (void *)1,
            .transmit = transmit,
        };
        const esp_netif_driver_ifconfig_t *ppp_driver_cfg = &driver_cfg;

        esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
        esp_netif_config_t netif_ppp_config = { .base = &base_netif_cfg,
                                                .driver = ppp_driver_cfg,
                                                .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP
                                              };

        s_netif = esp_netif_new(&netif_ppp_config);
        esp_netif_ppp_config_t netif_params;
        ESP_ERROR_CHECK(esp_netif_ppp_get_params(s_netif, &netif_params));
        netif_params.ppp_our_ip4_addr.addr = ESP_IP4TOADDR(192, 168, 11, 1);
        netif_params.ppp_their_ip4_addr.addr = ESP_IP4TOADDR(192, 168, 11, 2);
        netif_params.ppp_error_event_enabled = true;
        ESP_ERROR_CHECK(esp_netif_ppp_set_params(s_netif, &netif_params));
        if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL) != ESP_OK) {
            printf("Failed to register IP event handler");
        }
        if (esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_ppp_event, NULL) !=  ESP_OK) {
            printf("Failed to register NETIF_PPP_STATUS event handler");
        }


    }
    esp_at_port_write_data((uint8_t *)"CONNECT\r\n", strlen("CONNECT\r\n"));

    // set the callback function which will be called by AT port after receiving the input data
    esp_at_port_enter_specific(wait_data_callback);
    esp_netif_action_start(s_netif, 0, 0, 0);
    esp_netif_action_connected(s_netif, 0, 0, 0);

    while (xSemaphoreTake(at_sync_sema, pdMS_TO_TICKS(1000)) == pdFALSE) {
        printf(".");
    }
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_test_cereg(uint8_t *cmd_name)
{
    printf("%s: AT command <AT%s> is executed\r\n", __func__, cmd_name);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_cereg(uint8_t *cmd_name)
{
    printf("%s: AT command <AT%s> is executed\r\n", __func__, cmd_name);
    static uint8_t buffer[] = "+CEREG: 7,8\r\n";
    esp_at_port_write_data(buffer, sizeof(buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cereg(uint8_t num)
{
    printf("%s: AT command <AT%d> is executed\r\n", __func__, num);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_exe_cereg(uint8_t *cmd_name)
{
    printf("%s: AT command <AT%s> is executed\r\n", __func__, cmd_name);
    return ESP_AT_RESULT_CODE_OK;
}

static esp_err_t hello_get_handler(httpd_req_t *req)
{
    const char* resp_str = "Hello from ESP-AT HTTP Server!";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* resp_str = "ESP-AT HTTP Server is running";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t test_get_handler(httpd_req_t *req)
{
    const char* resp_str = "{\"status\":\"success\",\"message\":\"Test endpoint working\",\"timestamp\":12345}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t async_get_handler(httpd_req_t *req)
{
    printf("Starting async chunked response handler\r\n");

    // Set content type for plain text response
    httpd_resp_set_type(req, "text/plain");

    // Static counter to track requests
    static uint8_t req_count = 0;
    req_count++;

    // Send initial response with request count
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "=== Async Response #%d ===\r\n", req_count);
    httpd_resp_sendstr_chunk(req, buffer);

    // Long message broken into chunks
    const char* chunks[] = {
        "This is a simulated slow server response.\r\n",
        "Chunk 1: The ESP-AT HTTP server is demonstrating...\r\n",
        "Chunk 2: ...asynchronous chunked transfer encoding...\r\n",
        "Chunk 3: ...with artificial delays between chunks...\r\n",
        "Chunk 4: ...to simulate real-world network conditions.\r\n",
        "Chunk 5: Processing data... please wait...\r\n",
        "Chunk 6: Still processing... almost done...\r\n",
        "Chunk 7: Final chunk - transfer complete!\r\n",
        "=== END OF RESPONSE ===\r\n"
    };

    int num_chunks = sizeof(chunks) / sizeof(chunks[0]);

    // Send each chunk with delays
    for (int i = 0; i < num_chunks; i++) {
        // Add a delay to simulate slow processing
        vTaskDelay(pdMS_TO_TICKS(1500)); // 1.5 second delay between chunks

        // Add chunk number and timestamp
        snprintf(buffer, sizeof(buffer), "[%d/%d] [%d ms] %s",
                 i + 1, num_chunks, (int)(esp_timer_get_time() / 1000), chunks[i]);

        printf("Sending chunk %d: %s", i + 1, chunks[i]);
        httpd_resp_sendstr_chunk(req, buffer);
    }

    // Add final summary
    vTaskDelay(pdMS_TO_TICKS(500));
    snprintf(buffer, sizeof(buffer), "\r\nTransfer completed in %d chunks with delays.\r\n", num_chunks);
    httpd_resp_sendstr_chunk(req, buffer);

    // Send NULL to signal end of chunked transfer
    httpd_resp_sendstr_chunk(req, NULL);

    printf("Async chunked response completed\r\n");
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t test = {
    .uri       = "/test",
    .method    = HTTP_GET,
    .handler   = test_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t async_uri = {
    .uri       = "/async",
    .method    = HTTP_GET,
    .handler   = async_get_handler,
    .user_ctx  = NULL
};

static esp_err_t start_http_server(void)
{
    if (http_server != NULL) {
        printf("HTTP server already running\r\n");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.lru_purge_enable = true;

    printf("Starting HTTP server on port: %d\r\n", config.server_port);
    if (httpd_start(&http_server, &config) == ESP_OK) {
        printf("Registering URI handlers\r\n");
        httpd_register_uri_handler(http_server, &hello);
        httpd_register_uri_handler(http_server, &root);
        httpd_register_uri_handler(http_server, &test);
        httpd_register_uri_handler(http_server, &async_uri);
        return ESP_OK;
    }

    printf("Error starting HTTP server!\r\n");
    return ESP_FAIL;
}

static esp_err_t stop_http_server(void)
{
    if (http_server != NULL) {
        httpd_stop(http_server);
        http_server = NULL;
        printf("HTTP server stopped\r\n");
        return ESP_OK;
    }
    return ESP_OK;
}

/* HTTP Server AT Commands */
static uint8_t at_test_httpd(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "AT%s=<0/1> - Start/Stop HTTP server\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_httpd(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "+HTTPD:%d\r\n", http_server != NULL ? 1 : 0);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_httpd(uint8_t para_num)
{
    int32_t action = 0;
    if (esp_at_get_para_as_digit(0, &action) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (action == 1) {
        if (start_http_server() == ESP_OK) {
            printf("HTTP server started successfully\r\n");
            return ESP_AT_RESULT_CODE_OK;
        }
    } else if (action == 0) {
        if (stop_http_server() == ESP_OK) {
            return ESP_AT_RESULT_CODE_OK;
        }
    }

    return ESP_AT_RESULT_CODE_ERROR;
}

static uint8_t at_exe_httpd(uint8_t *cmd_name)
{
    // Default action: start server
    if (start_http_server() == ESP_OK) {
        printf("HTTP server started via execute command\r\n");
        return ESP_AT_RESULT_CODE_OK;
    }
    return ESP_AT_RESULT_CODE_ERROR;
}


static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+PPPD", at_test_cmd_test, at_query_cmd_test, at_setup_cmd_test, at_exe_cmd_test},
    {"+CEREG", at_test_cereg, at_query_cereg, at_setup_cereg, at_exe_cereg},
    {"+HTTPD", at_test_httpd, at_query_httpd, at_setup_httpd, at_exe_httpd},
};

bool esp_at_custom_cmd_register(void)
{
    return esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);
