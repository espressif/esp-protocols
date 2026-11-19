/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file net_connect_example.c
 * @brief Example demonstrating both net_connect() and the net_iface.h API.
 *
 * This example demonstrates how to:
 * 1. Initialize the network stack (esp_netif, NVS, default event loop)
 * 2. Register a NET_IFACE_EVENT listener to observe non-blocking state transitions
 * 3. Connect enabled interfaces concurrently via net_connect()
 * 4. Inspect active interface handles with net_iface_for_each() and net_get_netif_from_desc()
 * 5. Cleanly disconnect all interfaces with net_disconnect()
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

static const char *iface_type_str(net_iface_type_t type)
{
    switch (type) {
    case NET_IFACE_TYPE_WIFI_STA:
        return "Wi-Fi STA";
    case NET_IFACE_TYPE_ETH:
        return "Ethernet";
    case NET_IFACE_TYPE_THREAD:
        return "Thread";
    case NET_IFACE_TYPE_PPP:
        return "PPP";
    default:
        return "Unknown";
    }
}

static void on_net_iface_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;
    if (data == NULL) {
        return;
    }

    const char *type_name = iface_type_str(data->type);
    switch ((net_iface_event_id_t)event_id) {
    case NET_IFACE_EVENT_STARTED:
        ESP_LOGI(TAG, "[Event] %s driver started", type_name);
        break;
    case NET_IFACE_EVENT_CONNECTED:
        ESP_LOGI(TAG, "[Event] %s link up (waiting for IP)", type_name);
        break;
    case NET_IFACE_EVENT_READY:
        ESP_LOGI(TAG, "[Event] %s is READY (state=%d)", type_name, (int)data->state);
        break;
    case NET_IFACE_EVENT_LOST_IP:
        ESP_LOGW(TAG, "[Event] %s lost IP address", type_name);
        break;
    case NET_IFACE_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "[Event] %s link disconnected", type_name);
        break;
    case NET_IFACE_EVENT_STOPPED:
        ESP_LOGI(TAG, "[Event] %s stopped", type_name);
        break;
    case NET_IFACE_EVENT_FAILED:
        ESP_LOGE(TAG, "[Event] %s failed: %s", type_name, esp_err_to_name(data->err));
        break;
    default:
        break;
    }
}

static bool log_each_iface_cb(net_iface_handle_t h, void *ctx)
{
    (void)ctx;
    esp_netif_t *netif = net_iface_get_netif(h);
    const char *desc = (netif != NULL) ? esp_netif_get_desc(netif) : "none";
    ESP_LOGI(TAG, "Handle %s: state=%d, netif_desc=%s, last_err=%s",
             iface_type_str(net_iface_get_type(h)),
             (int)net_iface_get_state(h),
             desc != NULL ? desc : "none",
             esp_err_to_name(net_iface_get_last_error(h)));
    return true;
}

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

    // ===== Register NET_IFACE_EVENT callback =====
    ESP_ERROR_CHECK(net_iface_register_event_handler(on_net_iface_event, NULL));

    ESP_LOGI(TAG, "Network stack initialized");

#if CONFIG_NET_CONNECT_WIFI
    ESP_LOGI(TAG, "WiFi interface enabled in Kconfig (SSID: %s)", CONFIG_NET_CONNECT_WIFI_SSID);
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
    // Starts all enabled interfaces in parallel and waits for required ones.
    ESP_LOGI(TAG, "Connecting to network...");
    ret = net_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect network interfaces (err=0x%x)", ret);
        net_iface_unregister_event_handler(on_net_iface_event);
        return;
    }

    ESP_LOGI(TAG, "Network connection established successfully!");

    // ===== Inspect active interface handles using net_iface.h API =====
    ESP_ERROR_CHECK(net_iface_for_each(log_each_iface_cb, NULL));

    // ===== Demonstrate getting esp_netif by description (legacy API) =====
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

    // ===== Disconnect and clean up =====
    ESP_LOGI(TAG, "Disconnecting network interfaces...");
    ret = net_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect (err=0x%x)", ret);
    } else {
        ESP_LOGI(TAG, "All interfaces disconnected successfully");
    }

    ESP_ERROR_CHECK(net_iface_unregister_event_handler(on_net_iface_event));
    ESP_LOGI(TAG, "Example finished");
}
