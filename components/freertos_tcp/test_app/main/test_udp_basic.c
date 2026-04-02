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
    TEST_ASSERT_EQUAL(1, ret);

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

TEST_CASE("UDP: Send and receive (DNS round-trip)", "[freertos_tcp]")
{
    Socket_t sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    TEST_ASSERT_NOT_EQUAL(FREERTOS_INVALID_SOCKET, sock);

    TickType_t timeout = pdMS_TO_TICKS(5000);
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof(timeout));

    /* Minimal DNS query for "example.com" (type A, class IN).
     * This verifies the full UDP send→network→receive path. */
    static const uint8_t dns_query[] = {
        0x00, 0x42, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 'e', 'x', 'a',
        'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm',
        0x00, 0x00, 0x01, 0x00, 0x01
    };

    NetworkEndPoint_t *pxEndPoint = FreeRTOS_FirstEndPoint(NULL);
    TEST_ASSERT_NOT_NULL(pxEndPoint);

    struct freertos_sockaddr dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_len = sizeof(struct freertos_sockaddr);
    dest_addr.sin_family = FREERTOS_AF_INET;
    dest_addr.sin_port = FreeRTOS_htons(53);
    dest_addr.sin_address.ulIP_IPv4 = pxEndPoint->ipv4_settings.ulDNSServerAddresses[0];

    BaseType_t sent = FreeRTOS_sendto(sock, dns_query, sizeof(dns_query), 0,
                                      &dest_addr, sizeof(dest_addr));
    TEST_ASSERT_EQUAL(sizeof(dns_query), sent);
    ESP_LOGI(TAG, "Sent %d bytes to DNS server", sent);

    char rx_buffer[512] = {0};
    struct freertos_sockaddr from_addr;
    socklen_t from_len = sizeof(from_addr);
    BaseType_t received = FreeRTOS_recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0,
                                            &from_addr, &from_len);
    TEST_ASSERT_GREATER_THAN(0, received);
    ESP_LOGI(TAG, "Received %d bytes from DNS server", received);

    /* Verify it's a DNS response (QR bit set, matching ID 0x0042) */
    TEST_ASSERT_EQUAL_HEX8(0x00, (uint8_t)rx_buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x42, (uint8_t)rx_buffer[1]);
    TEST_ASSERT_TRUE((uint8_t)rx_buffer[2] & 0x80);

    FreeRTOS_closesocket(sock);
    ESP_LOGI(TAG, "UDP send/receive (DNS round-trip): PASS");
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
