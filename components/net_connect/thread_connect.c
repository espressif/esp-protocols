/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_vfs_eventfd.h"
#include "net_connect_private.h"
#include "net_connect.h"
#include "protocol_examples_thread_config.h"
#include "esp_log.h"
#include <string.h>

#include <esp_openthread_cli.h>
#include <esp_openthread_lock.h>
#include <esp_openthread_netif_glue.h>
#include <esp_openthread_types.h>
#include <esp_openthread.h>
#include <openthread/dataset.h>
#include <openthread/logging.h>

static TaskHandle_t s_ot_task_handle = NULL;
static esp_netif_t *s_openthread_netif = NULL;
static SemaphoreHandle_t s_semph_thread_attached = NULL;
static SemaphoreHandle_t s_semph_thread_set_dns_server = NULL;
static const char *TAG = "net_connect_thread";

static void thread_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                 void* event_data)
{
    if (event_base == OPENTHREAD_EVENT) {
        if (event_id == OPENTHREAD_EVENT_ATTACHED) {
            xSemaphoreGive(s_semph_thread_attached);
        } else if (event_id == OPENTHREAD_EVENT_SET_DNS_SERVER) {
            xSemaphoreGive(s_semph_thread_set_dns_server);
        }
    }
}

static void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_OPENTHREAD();
    esp_netif_config.if_desc = NET_CONNECT_NETIF_DESC_THREAD;
    esp_netif_config_t cfg = {
        .base = &esp_netif_config,
        .stack = &g_esp_netif_netstack_default_openthread,
    };
    s_openthread_netif = esp_netif_new(&cfg);
    assert(s_openthread_netif != NULL);

    // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));
    ESP_ERROR_CHECK(esp_netif_attach(s_openthread_netif, esp_openthread_netif_glue_init(&config)));
    esp_openthread_lock_acquire(portMAX_DELAY);
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
    esp_openthread_cli_init();
    esp_openthread_cli_create_task();
    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    if (error != OT_ERROR_NONE) {
        ESP_ERROR_CHECK(esp_openthread_auto_start(NULL));
    } else {
        ESP_ERROR_CHECK(esp_openthread_auto_start(&dataset));
    }
    esp_openthread_lock_release();

    // Run the main loop
    esp_openthread_launch_mainloop();

    // Clean up
    // Clear the task handle before cleanup to prevent double-cleanup in net_connect_thread_shutdown()
    s_ot_task_handle = NULL;
    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(s_openthread_netif);
    s_openthread_netif = NULL;
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

/* tear down connection, release resources */
void net_connect_thread_shutdown(void)
{
    if (s_ot_task_handle != NULL) {
        vTaskDelete(s_ot_task_handle);
        s_ot_task_handle = NULL;
        // Only clean up resources if we deleted the task
        // If s_ot_task_handle was NULL, the task already cleaned up
        esp_openthread_netif_glue_deinit();
        esp_netif_destroy(s_openthread_netif);
        s_openthread_netif = NULL;
        esp_vfs_eventfd_unregister();
    }
    esp_event_handler_unregister(OPENTHREAD_EVENT, ESP_EVENT_ANY_ID, thread_event_handler);
    if (s_semph_thread_set_dns_server != NULL) {
        vSemaphoreDelete(s_semph_thread_set_dns_server);
        s_semph_thread_set_dns_server = NULL;
    }
    if (s_semph_thread_attached != NULL) {
        vSemaphoreDelete(s_semph_thread_attached);
        s_semph_thread_attached = NULL;
    }
}

esp_err_t net_connect_thread_connect(void)
{
    s_semph_thread_attached = xSemaphoreCreateBinary();
    if (s_semph_thread_attached == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_semph_thread_set_dns_server = xSemaphoreCreateBinary();
    if (s_semph_thread_set_dns_server == NULL) {
        vSemaphoreDelete(s_semph_thread_attached);
        s_semph_thread_attached = NULL;
        return ESP_ERR_NO_MEM;
    }
    // 4 eventfds might be used for Thread
    // * netif
    // * ot task queue
    // * radio driver
    // * border router
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 4,
    };
    esp_err_t ret = esp_vfs_eventfd_register(&eventfd_config);
    if (ret != ESP_OK) {
        vSemaphoreDelete(s_semph_thread_attached);
        s_semph_thread_attached = NULL;
        vSemaphoreDelete(s_semph_thread_set_dns_server);
        s_semph_thread_set_dns_server = NULL;
        ESP_LOGE(TAG, "Failed to register eventfd VFS");
        return ret;
    }
    ESP_ERROR_CHECK(esp_event_handler_register(OPENTHREAD_EVENT, ESP_EVENT_ANY_ID, thread_event_handler, NULL));
    if (xTaskCreate(ot_task_worker, "ot_br_main", CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE, NULL, 5, &s_ot_task_handle) != pdPASS) {
        esp_event_handler_unregister(OPENTHREAD_EVENT, ESP_EVENT_ANY_ID, thread_event_handler);
        esp_vfs_eventfd_unregister();
        vSemaphoreDelete(s_semph_thread_attached);
        s_semph_thread_attached = NULL;
        vSemaphoreDelete(s_semph_thread_set_dns_server);
        s_semph_thread_set_dns_server = NULL;
        ESP_LOGE(TAG, "Failed to create openthread task");
        return ESP_FAIL;
    }
    xSemaphoreTake(s_semph_thread_attached, portMAX_DELAY);
    // Wait 1s for the Thread device to set its DNS server with the NAT64 prefix.
    if (xSemaphoreTake(s_semph_thread_set_dns_server, 1000 / portTICK_PERIOD_MS) != pdPASS) {
        ESP_LOGW(TAG, "DNS server is not set for the Thread device, might be unable to access the Internet");
    }
    return ESP_OK;
}
