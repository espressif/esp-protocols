/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file net_connect_example.c
 * @brief Example demonstrating basic usage of the net_connect component.
 *
 * This example demonstrates how to use the net_connect component to establish
 * network connectivity using WiFi, Ethernet, Thread, or PPP interfaces.
 * All configuration is done via Kconfig menuconfig options.
 *
 * The example:
 * 1. Initializes the network stack (esp_netif, NVS, event loop)
 * 2. Calls net_connect() to establish connection using Kconfig settings
 * 3. Demonstrates how to get the network interface
 * 4. Optionally disconnects after a delay
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "net_connect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "net_connect_example";

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting net_connect example...");

    // ===== Initialize network interfaces =====
    ESP_ERROR_CHECK(esp_netif_init());

    // ===== Initialize NVS (required for Wi-Fi and Ethernet) =====
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ===== Initialize event loop =====
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Network stack initialized");

#if CONFIG_NET_CONNECT_WIFI
    ESP_LOGI(TAG, "WiFi interface enabled in Kconfig");
    ESP_LOGI(TAG, "WiFi SSID: %s", CONFIG_NET_CONNECT_WIFI_SSID);
#endif

#if CONFIG_NET_CONNECT_ETHERNET
    ESP_LOGI(TAG, "Ethernet interface enabled in Kconfig");
#endif

#if CONFIG_NET_CONNECT_THREAD
    ESP_LOGI(TAG, "Thread interface enabled in Kconfig");
#endif

#if CONFIG_NET_CONNECT_PPP
    ESP_LOGI(TAG, "PPP interface enabled in Kconfig");
#endif

    // ===== Connect using unified API =====
    // This function reads configuration from Kconfig and connects all
    // enabled interfaces (WiFi, Ethernet, Thread, PPP)
    ESP_LOGI(TAG, "Connecting to network...");
    ret = net_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect network interfaces (err=0x%x)", ret);
        return;
    }

    ESP_LOGI(TAG, "Network connection established successfully!");

    // ===== Demonstrate getting network interface =====
    // The net_connect() function already prints IP addresses, but we can
    // also get the interface directly if needed
#if CONFIG_NET_CONNECT_WIFI
    esp_netif_t *wifi_netif = net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_STA);
    if (wifi_netif != NULL) {
        ESP_LOGI(TAG, "WiFi netif retrieved: %s", esp_netif_get_desc(wifi_netif));
    }
#endif

#if CONFIG_NET_CONNECT_ETHERNET
    esp_netif_t *eth_netif = net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_ETH);
    if (eth_netif != NULL) {
        ESP_LOGI(TAG, "Ethernet netif retrieved: %s", esp_netif_get_desc(eth_netif));
    }
#endif

#if CONFIG_NET_CONNECT_THREAD
    esp_netif_t *thread_netif = net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_THREAD);
    if (thread_netif != NULL) {
        ESP_LOGI(TAG, "Thread netif retrieved: %s", esp_netif_get_desc(thread_netif));
    }
#endif

#if CONFIG_NET_CONNECT_PPP
    esp_netif_t *ppp_netif = net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_PPP);
    if (ppp_netif != NULL) {
        ESP_LOGI(TAG, "PPP netif retrieved: %s", esp_netif_get_desc(ppp_netif));
    }
#endif

    // ===== Keep connection alive for demonstration =====
    ESP_LOGI(TAG, "Network connection active. Waiting 30 seconds...");
    vTaskDelay(pdMS_TO_TICKS(30000));

    // ===== Optionally: Disconnect after demonstration =====
    ESP_LOGI(TAG, "Disconnecting network interfaces...");
    ret = net_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect (err=0x%x)", ret);
    } else {
        ESP_LOGI(TAG, "All interfaces disconnected successfully");
    }

    ESP_LOGI(TAG, "Example finished");
}
