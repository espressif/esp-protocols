/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ppp_connect_example.c
 * @brief Example demonstrating PPP connection using the net_connect component.
 *
 * This example demonstrates how to use the net_connect component to establish
 * a PPP (Point-to-Point Protocol) connection over serial (UART or USB CDC).
 * All configuration is done via Kconfig menuconfig options.
 *
 * The example:
 * 1. Initializes the network stack (esp_netif, NVS, event loop)
 * 2. Calls net_connect() to establish PPP connection using Kconfig settings
 * 3. Displays connection status and IP addresses (IPv4 and IPv6)
 * 4. Demonstrates how to get the network interface
 * 5. Optionally disconnects after a delay
 *
 * Prerequisites:
 * - A Linux machine with pppd installed running as PPP server
 * - Serial connection (UART or USB CDC) between ESP32 and Linux machine
 * - See README.md for detailed PPP server setup instructions
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "net_connect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_NET_CONNECT_IPV6
#include "lwip/opt.h"
#endif

static const char *TAG = "ppp_connect_example";

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Starting PPP connection example...");
    ESP_LOGI(TAG, "This example demonstrates PPP connection over serial");

#if CONFIG_NET_CONNECT_PPP_DEVICE_USB
    ESP_LOGI(TAG, "PPP device: USB CDC");
#elif CONFIG_NET_CONNECT_PPP_DEVICE_UART
    ESP_LOGI(TAG, "PPP device: UART");
    ESP_LOGI(TAG, "UART TX Pin: %d, RX Pin: %d, Baudrate: %d",
             CONFIG_NET_CONNECT_UART_TX_PIN,
             CONFIG_NET_CONNECT_UART_RX_PIN,
             CONFIG_NET_CONNECT_UART_BAUDRATE);
#endif

    // ===== Initialize network interfaces =====
    ESP_ERROR_CHECK(esp_netif_init());

    // ===== Initialize NVS (required for some network features) =====
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

#if CONFIG_NET_CONNECT_IPV6
    ESP_LOGI(TAG, "IPv6 support enabled");
#endif

    // ===== Connect using unified API =====
    // This function reads configuration from Kconfig and establishes PPP connection
    ESP_LOGI(TAG, "Connecting to PPP server...");
    ESP_LOGI(TAG, "Make sure pppd is running on the host machine");
    ret = net_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to PPP server (err=0x%x)", ret);
        ESP_LOGE(TAG, "Please check:");
        ESP_LOGE(TAG, "  1. Serial connection is properly connected");
        ESP_LOGE(TAG, "  2. pppd server is running on host machine");
        ESP_LOGE(TAG, "  3. Device name matches (e.g., /dev/ttyACM0 or /dev/ttyUSB0)");
        ESP_LOGE(TAG, "  4. See README.md for detailed setup instructions");
        return;
    }

    ESP_LOGI(TAG, "PPP connection established successfully!");

    // ===== Demonstrate getting network interface =====
    // The net_connect() function already prints IP addresses, but we can
    // also get the interface directly if needed
    esp_netif_t *ppp_netif = net_get_netif_from_desc(NET_CONNECT_NETIF_DESC_PPP);
    if (ppp_netif != NULL) {
        ESP_LOGI(TAG, "PPP netif retrieved: %s", esp_netif_get_desc(ppp_netif));

        // Display IPv4 address if available
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(ppp_netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "IPv4 Address: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "IPv4 Netmask: " IPSTR, IP2STR(&ip_info.netmask));
            ESP_LOGI(TAG, "IPv4 Gateway: " IPSTR, IP2STR(&ip_info.gw));

            // Display DNS servers
            esp_netif_dns_info_t dns_info;
            if (esp_netif_get_dns_info(ppp_netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
                ESP_LOGI(TAG, "DNS Server: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            }
        }

#if CONFIG_NET_CONNECT_IPV6
        // Display IPv6 addresses if available
        // Network interfaces can have multiple IPv6 addresses (e.g., link-local and global)
        // Use LWIP_IPV6_NUM_ADDRESSES macro which defines the maximum number of IPv6 addresses per netif
        esp_ip6_addr_t ip6[LWIP_IPV6_NUM_ADDRESSES];
        int num_ip6 = esp_netif_get_all_ip6(ppp_netif, ip6);
        for (int i = 0; i < num_ip6; i++) {
            ESP_LOGI(TAG, "IPv6 Address[%d]: " IPV6STR, i, IPV62STR(ip6[i]));
        }
#endif
    } else {
        ESP_LOGW(TAG, "Could not retrieve PPP netif");
    }

    // ===== Keep connection alive for demonstration =====
    ESP_LOGI(TAG, "PPP connection active. Waiting 30 seconds...");
    ESP_LOGI(TAG, "You can now test network connectivity from the ESP32");
    vTaskDelay(pdMS_TO_TICKS(30000));

    // ===== Optionally: Disconnect after demonstration =====
    ESP_LOGI(TAG, "Disconnecting PPP connection...");
    ret = net_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect (err=0x%x)", ret);
    } else {
        ESP_LOGI(TAG, "PPP connection disconnected successfully");
    }

    ESP_LOGI(TAG, "Example finished");
}
