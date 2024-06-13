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

static const char *TAG = "test_udp";

TEST_CASE("UDP: Create and close socket", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    BaseType_t ret = FreeRTOS_closesocket(sock);
    TEST_ASSERT_EQUAL(0, ret);

    ESP_LOGI(TAG, "UDP socket create/close: PASS");
}

TEST_CASE("UDP: Bind to port", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    struct freertos_sockaddr bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_len = sizeof(struct freertos_sockaddr);
    bind_addr.sin_family = FREERTOS_AF_INET;
    bind_addr.sin_port = FreeRTOS_htons(12345);
    bind_addr.sin_address.ulIP_IPv4 = FreeRTOS_htonl(FREERTOS_INADDR_ANY);

    BaseType_t ret = FreeRTOS_bind(sock, &bind_addr, sizeof(bind_addr));
    TEST_ASSERT_EQUAL(0, ret);

    FreeRTOS_closesocket(sock);

    ESP_LOGI(TAG, "UDP bind: PASS");
}

TEST_CASE("UDP: Send and receive (loopback)", "[freertos_tcp]")
{
    // Create sending socket
    Socket_t send_sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, send_sock);

    // Create receiving socket
    Socket_t recv_sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, recv_sock);

    // Bind receive socket to a specific port
    struct freertos_sockaddr recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_len = sizeof(struct freertos_sockaddr);
    recv_addr.sin_family = FREERTOS_AF_INET;
    recv_addr.sin_port = FreeRTOS_htons(12346);
    recv_addr.sin_address.ulIP_IPv4 = FreeRTOS_htonl(FREERTOS_INADDR_ANY);

    BaseType_t ret = FreeRTOS_bind(recv_sock, &recv_addr, sizeof(recv_addr));
    TEST_ASSERT_EQUAL(0, ret);

    // Set receive timeout
    TickType_t timeout = pdMS_TO_TICKS(5000);
    FreeRTOS_setsockopt(recv_sock, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Get local IP address
    NetworkEndPoint_t *pxEndPoint = FreeRTOS_FirstEndPoint(NULL);
    TEST_ASSERT_NOT_NULL(pxEndPoint);
    uint32_t local_ip = pxEndPoint->ipv4_settings.ulIPAddress;

    // Send data to ourselves
    struct freertos_sockaddr dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_len = sizeof(struct freertos_sockaddr);
    dest_addr.sin_family = FREERTOS_AF_INET;
    dest_addr.sin_port = FreeRTOS_htons(12346);
    dest_addr.sin_address.ulIP_IPv4 = local_ip;

    const char *test_data = "Hello UDP";
    BaseType_t sent = FreeRTOS_sendto(send_sock, test_data, strlen(test_data), 0,
                                      &dest_addr, sizeof(dest_addr));
    TEST_ASSERT_EQUAL(strlen(test_data), sent);

    ESP_LOGI(TAG, "Sent %d bytes", sent);

    // Receive data
    char rx_buffer[64] = {0};
    struct freertos_sockaddr from_addr;
    socklen_t from_len = sizeof(from_addr);
    BaseType_t received = FreeRTOS_recvfrom(recv_sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                                            &from_addr, &from_len);
    TEST_ASSERT_GREATER_THAN(0, received);
    TEST_ASSERT_EQUAL_STRING(test_data, rx_buffer);

    ESP_LOGI(TAG, "Received %d bytes: %s", received, rx_buffer);

    FreeRTOS_closesocket(send_sock);
    FreeRTOS_closesocket(recv_sock);

    ESP_LOGI(TAG, "UDP send/receive: PASS");
}

TEST_CASE("UDP: Socket options", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    // Set and verify timeout
    TickType_t timeout = pdMS_TO_TICKS(5000);
    BaseType_t ret = FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof(timeout));
    TEST_ASSERT_EQUAL(0, ret);

    FreeRTOS_closesocket(sock);

    ESP_LOGI(TAG, "UDP socket options: PASS");
}
