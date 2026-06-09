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

static const char *TAG = "test_dns";

TEST_CASE("DNS: Resolve hostname", "[freertos_tcp]")
{
    struct freertos_addrinfo hints;
    struct freertos_addrinfo *results = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = FREERTOS_AF_INET;

    BaseType_t rc = FreeRTOS_getaddrinfo(CONFIG_TEST_DNS_HOSTNAME, NULL, &hints, &results);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(results);
    TEST_ASSERT_EQUAL(FREERTOS_AF_INET4, results->ai_family);
    TEST_ASSERT_NOT_EQUAL(0, results->ai_addr->sin_address.ulIP_IPv4);

    uint32_t ip = results->ai_addr->sin_address.ulIP_IPv4;
    ESP_LOGI(TAG, "Resolved %s to: %d.%d.%d.%d",
             CONFIG_TEST_DNS_HOSTNAME,
             (int)(ip & 0xFF),
             (int)((ip >> 8) & 0xFF),
             (int)((ip >> 16) & 0xFF),
             (int)((ip >> 24) & 0xFF));

    FreeRTOS_freeaddrinfo(results);

    ESP_LOGI(TAG, "DNS resolution: PASS");
}

TEST_CASE("DNS: Resolve multiple hostnames", "[freertos_tcp]")
{
    const char *hostnames[] = {
        "example.com",
        "google.com",
        "espressif.com"
    };

    for (int i = 0; i < 3; i++) {
        struct freertos_addrinfo hints;
        struct freertos_addrinfo *results = NULL;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = FREERTOS_AF_INET;

        BaseType_t rc = FreeRTOS_getaddrinfo(hostnames[i], NULL, &hints, &results);
        TEST_ASSERT_EQUAL(0, rc);
        TEST_ASSERT_NOT_NULL(results);

        uint32_t ip = results->ai_addr->sin_address.ulIP_IPv4;
        ESP_LOGI(TAG, "Resolved %s to: %d.%d.%d.%d",
                 hostnames[i],
                 (int)(ip & 0xFF),
                 (int)((ip >> 8) & 0xFF),
                 (int)((ip >> 16) & 0xFF),
                 (int)((ip >> 24) & 0xFF));

        FreeRTOS_freeaddrinfo(results);
    }

    ESP_LOGI(TAG, "Multiple DNS resolution: PASS");
}

TEST_CASE("DNS: Cache functionality", "[freertos_tcp]")
{
    struct freertos_addrinfo hints;
    struct freertos_addrinfo *results = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = FREERTOS_AF_INET;

    // First resolution - should go to DNS server
    TickType_t start = xTaskGetTickCount();
    BaseType_t rc = FreeRTOS_getaddrinfo(CONFIG_TEST_DNS_HOSTNAME, NULL, &hints, &results);
    TickType_t first_time = xTaskGetTickCount() - start;
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(results);

    uint32_t first_ip = results->ai_addr->sin_address.ulIP_IPv4;
    FreeRTOS_freeaddrinfo(results);
    results = NULL;

    // Second resolution - should hit cache (faster)
    start = xTaskGetTickCount();
    rc = FreeRTOS_getaddrinfo(CONFIG_TEST_DNS_HOSTNAME, NULL, &hints, &results);
    TickType_t second_time = xTaskGetTickCount() - start;
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(results);

    uint32_t second_ip = results->ai_addr->sin_address.ulIP_IPv4;
    TEST_ASSERT_EQUAL(first_ip, second_ip);

    FreeRTOS_freeaddrinfo(results);

    ESP_LOGI(TAG, "First resolution took %d ticks, second took %d ticks",
             (int)first_time, (int)second_time);
    ESP_LOGI(TAG, "DNS cache functionality: PASS");
}

TEST_CASE("DHCP: Verify IP obtained", "[freertos_tcp]")
{
    NetworkEndPoint_t *pxEndPoint = FreeRTOS_FirstEndPoint(NULL);
    TEST_ASSERT_NOT_NULL(pxEndPoint);

    uint32_t ip = pxEndPoint->ipv4_settings.ulIPAddress;
    uint32_t netmask = pxEndPoint->ipv4_settings.ulNetMask;
    uint32_t gateway = pxEndPoint->ipv4_settings.ulGatewayAddress;

    TEST_ASSERT_NOT_EQUAL(0, ip);
    TEST_ASSERT_NOT_EQUAL(0, netmask);
    TEST_ASSERT_NOT_EQUAL(0, gateway);

    ESP_LOGI(TAG, "IP Address: %d.%d.%d.%d",
             (int)(ip & 0xFF),
             (int)((ip >> 8) & 0xFF),
             (int)((ip >> 16) & 0xFF),
             (int)((ip >> 24) & 0xFF));

    ESP_LOGI(TAG, "Netmask: %d.%d.%d.%d",
             (int)(netmask & 0xFF),
             (int)((netmask >> 8) & 0xFF),
             (int)((netmask >> 16) & 0xFF),
             (int)((netmask >> 24) & 0xFF));

    ESP_LOGI(TAG, "Gateway: %d.%d.%d.%d",
             (int)(gateway & 0xFF),
             (int)((gateway >> 8) & 0xFF),
             (int)((gateway >> 16) & 0xFF),
             (int)((gateway >> 24) & 0xFF));

    ESP_LOGI(TAG, "DHCP verification: PASS");
}
