/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "unity.h"
#include "esp_log.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "test_tcp";

TEST_CASE("TCP: Create and close socket", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    BaseType_t ret = FreeRTOS_closesocket(sock);
    TEST_ASSERT_EQUAL(0, ret);

    ESP_LOGI(TAG, "TCP socket create/close: PASS");
}

TEST_CASE("TCP: Connect to server and send HTTP request", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    // Set socket timeout
    TickType_t timeout = pdMS_TO_TICKS(10000);
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof(timeout));
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_SNDTIMEO, &timeout, sizeof(timeout));

    // Resolve hostname
    struct freertos_addrinfo hints = { .ai_family = FREERTOS_AF_INET };
    struct freertos_addrinfo *results = NULL;
    BaseType_t rc = FreeRTOS_getaddrinfo(CONFIG_TEST_DNS_HOSTNAME, NULL, &hints, &results);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(results);
    TEST_ASSERT_EQUAL(FREERTOS_AF_INET4, results->ai_family);

    // Connect to server
    struct freertos_sockaddr addr;
    addr.sin_len = sizeof(struct freertos_sockaddr);
    addr.sin_family = FREERTOS_AF_INET;
    addr.sin_port = FreeRTOS_htons(CONFIG_TEST_TCP_SERVER_PORT);
    addr.sin_address.ulIP_IPv4 = results->ai_addr->sin_address.ulIP_IPv4;

    FreeRTOS_freeaddrinfo(results);

    // Wait for ARP resolution if needed
    NetworkEndPoint_t *pxEndPoint = FreeRTOS_FindGateWay(ipTYPE_IPv4);
    if (pxEndPoint != NULL && pxEndPoint->ipv4_settings.ulGatewayAddress != 0U) {
        xARPWaitResolution(pxEndPoint->ipv4_settings.ulGatewayAddress, pdMS_TO_TICKS(1000U));
    }

    rc = FreeRTOS_connect(sock, &addr, sizeof(addr));
    TEST_ASSERT_EQUAL(0, rc);

    ESP_LOGI(TAG, "Connected to server");

    // Send HTTP GET request
    const char *request = "GET / HTTP/1.1\r\nHost: " CONFIG_TEST_DNS_HOSTNAME "\r\nConnection: close\r\n\r\n";
    BaseType_t sent = FreeRTOS_send(sock, request, strlen(request), 0);
    TEST_ASSERT_GREATER_THAN(0, sent);

    ESP_LOGI(TAG, "Sent %d bytes", sent);

    // Receive response
    char rx_buffer[256] = {0};
    BaseType_t received = FreeRTOS_recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    TEST_ASSERT_GREATER_THAN(0, received);

    rx_buffer[received] = 0;
    ESP_LOGI(TAG, "Received %d bytes", received);
    ESP_LOGD(TAG, "Response: %.100s...", rx_buffer);

    // Verify HTTP response
    TEST_ASSERT_NOT_NULL(strstr(rx_buffer, "HTTP/"));

    FreeRTOS_closesocket(sock);

    ESP_LOGI(TAG, "TCP connect and HTTP request: PASS");
}

TEST_CASE("TCP: Socket options", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    // Set and verify receive timeout
    TickType_t rx_timeout = pdMS_TO_TICKS(5000);
    BaseType_t ret = FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &rx_timeout, sizeof(rx_timeout));
    TEST_ASSERT_EQUAL(0, ret);

    // Set and verify send timeout
    TickType_t tx_timeout = pdMS_TO_TICKS(5000);
    ret = FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_SNDTIMEO, &tx_timeout, sizeof(tx_timeout));
    TEST_ASSERT_EQUAL(0, ret);

    FreeRTOS_closesocket(sock);

    ESP_LOGI(TAG, "TCP socket options: PASS");
}
