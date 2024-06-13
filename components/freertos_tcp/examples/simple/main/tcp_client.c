/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

static const char *TAG = "AFpT_tcp_client";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    Socket_t sock =  FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (sock == FREERTOS_INVALID_SOCKET) {
        ESP_LOGE(TAG, "Unable to create socket");
        return;
    }
    struct freertos_sockaddr addr;
    struct freertos_addrinfo *results = NULL;
    struct freertos_addrinfo hints = { .ai_family = FREERTOS_AF_INET };
    NetworkEndPoint_t *pxEndPoint = FreeRTOS_FindGateWay(ipTYPE_IPv4);

    if ((pxEndPoint != NULL) && (pxEndPoint->ipv4_settings.ulGatewayAddress != 0U)) {
        xARPWaitResolution(pxEndPoint->ipv4_settings.ulGatewayAddress, pdMS_TO_TICKS(1000U));
    }
    BaseType_t rc = FreeRTOS_getaddrinfo(
                        CONFIG_EXAMPLE_HOSTNAME,  /* The node. */
                        NULL,        /* const char *pcService: ignored for now. */
                        &hints,     /* If not NULL: preferences. */
                        &results);  /* An allocated struct, containing the results. */
    ESP_LOGI(TAG, "FreeRTOS_getaddrinfo() returned rc: %d", rc);
    if ((rc != 0) || (results == NULL) || results->ai_family != FREERTOS_AF_INET4) {
        ESP_LOGI(TAG, "Failed to lookup IPv4");
        return;
    }
    addr.sin_len = sizeof(struct freertos_sockaddr);
    addr.sin_family = FREERTOS_AF_INET;
    addr.sin_port = FreeRTOS_htons(CONFIG_EXAMPLE_PORT);
    addr.sin_address.ulIP_IPv4 = results->ai_addr->sin_address.ulIP_IPv4;;
    rc = FreeRTOS_connect(sock, &addr, sizeof(addr));
    ESP_LOGI(TAG, "Connecting to %" PRIx32 " %d", addr.sin_address.ulIP_IPv4, rc);
    const char *payload = "GET / HTTP/1.1\r\n\r\n";
    char rx_buffer[128] = {0};
    rc = FreeRTOS_send(sock, payload, strlen(payload), 0);
    if (rc < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: rc: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Sending finished with: rc %d", rc);
    rc = FreeRTOS_recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (rc < 0) {
        ESP_LOGE(TAG, "Error occurred during receiving: rc %d", rc);
        return;
    } else {
        ESP_LOGI(TAG, "Receiving finished with: rc %d", rc);
        if (rc > 0) {
            rx_buffer[rc] = 0; // Null-terminate whatever we received and treat like a string
            ESP_LOGI(TAG, "%s", rx_buffer);
        }
    }
    FreeRTOS_closesocket(sock);
}
