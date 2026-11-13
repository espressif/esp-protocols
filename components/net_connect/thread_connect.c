/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * OpenThread driver.
 *
 * start() registers eventfd and launches the OpenThread task. It does not
 * wait for the network. The task creates the netif, then joins if auto-connect
 * is on. net_iface.c marks the interface ready on OPENTHREAD_EVENT_ATTACHED.
 */

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs_eventfd.h"
#include "net_connect.h"
#include "net_iface_priv.h"
#include "protocol_examples_thread_config.h"

#include <esp_openthread_cli.h>
#include <esp_openthread_lock.h>
#include <esp_openthread_netif_glue.h>
#include <esp_openthread_types.h>
#include <esp_openthread.h>
#include <openthread/dataset.h>
#include <openthread/ip6.h>
#include <openthread/logging.h>
#include <openthread/thread.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "net_connect_thread";

static TaskHandle_t s_ot_task_handle = NULL;
static SemaphoreHandle_t s_ot_stop_sem = NULL;
/* Set by stop() so the worker skips the main loop if it has not entered yet. */
static volatile bool s_ot_stop_requested = false;
/*
 * xTaskCreate() can run the new task before it stores the handle.
 * The worker waits for this before it clears that handle.
 */
static volatile bool s_ot_task_published = false;
static esp_netif_t *s_openthread_netif = NULL;
static bool s_eventfd_owned = false;
static bool s_glue_attached = false;

/* Log when Thread publishes a DNS server. This does not block any task. */
static void on_thread_dns_server(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;
    if (event_id == OPENTHREAD_EVENT_SET_DNS_SERVER) {
        ESP_LOGI(TAG, "Thread DNS server is set");
    }
}

/* Free the netif and glue. Safe to call twice. */
static void thread_release_netif(void)
{
    if (s_glue_attached) {
        esp_openthread_netif_glue_deinit();
        s_glue_attached = false;
    }
    if (s_openthread_netif != NULL) {
        esp_netif_destroy(s_openthread_netif);
        s_openthread_netif = NULL;
    }
}

/* Unregister eventfd if this driver registered it. */
static void thread_release_eventfd(void)
{
    if (s_eventfd_owned) {
        esp_vfs_eventfd_unregister();
        s_eventfd_owned = false;
    }
}

/* Free every Thread resource this driver owns. Safe to call twice. */
static void thread_release_resources(void)
{
    thread_release_netif();
    thread_release_eventfd();
}

/*
 * Read the active dataset and start Thread.
 * Must run with the OpenThread lock held by the caller.
 */
static esp_err_t thread_auto_start_locked(void)
{
    otInstance *instance = esp_openthread_get_instance();
    if (instance == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(instance, &dataset);
    if (error != OT_ERROR_NONE) {
        return esp_openthread_auto_start(NULL);
    }
    return esp_openthread_auto_start(&dataset);
}

/* Tell stop() that this task has finished cleaning up and is about to exit. */
static void ot_signal_task_exited(void)
{
    while (!s_ot_task_published) {
        taskYIELD();
    }
    s_ot_task_handle = NULL;
    if (s_ot_stop_sem != NULL) {
        xSemaphoreGive(s_ot_stop_sem);
    }
}

/*
 * OpenThread worker.
 * Creates the netif, starts the stack, reports "started", and optionally joins
 * the network. Then it stays in the OpenThread main loop until stop() asks it
 * to leave. This task shuts OpenThread down itself. stop() must not delete it.
 */
static void ot_task_worker(void *arg)
{
    net_iface_handle_t self = (net_iface_handle_t)arg;

    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_OPENTHREAD();
    base.if_desc = (self != NULL && self->cfg.desc != NULL) ? self->cfg.desc : NET_CONNECT_NETIF_DESC_THREAD;
    if (self != NULL && self->cfg.route_prio > 0) {
        base.route_prio = self->cfg.route_prio;
    } else {
        base.route_prio = NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO;
    }
    esp_netif_config_t cfg = {
        .base = &base,
        .stack = &g_esp_netif_netstack_default_openthread,
    };

    s_openthread_netif = esp_netif_new(&cfg);
    if (s_openthread_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create Thread netif");
        net_iface_notify_failed(self, ESP_ERR_NO_MEM);
        ot_signal_task_exited();
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = esp_openthread_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_openthread_init failed: %s", esp_err_to_name(err));
        thread_release_resources();
        net_iface_notify_failed(self, err);
        ot_signal_task_exited();
        vTaskDelete(NULL);
        return;
    }

    void *glue = esp_openthread_netif_glue_init(&config);
    if (glue == NULL || esp_netif_attach(s_openthread_netif, glue) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach the Thread netif");
        if (glue != NULL) {
            s_glue_attached = true;
        }
        /* Drop the netif before OpenThread, and drop eventfd after OpenThread. */
        thread_release_netif();
        esp_openthread_deinit();
        thread_release_eventfd();
        net_iface_notify_failed(self, ESP_FAIL);
        ot_signal_task_exited();
        vTaskDelete(NULL);
        return;
    }
    s_glue_attached = true;

    esp_openthread_lock_acquire(portMAX_DELAY);
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
    esp_openthread_cli_init();
    esp_openthread_cli_create_task();
    esp_openthread_lock_release();

    net_iface_notify_started(self, s_openthread_netif);

    /*
     * The startup task may also call connect if it already sees STARTED.
     * Calling connect only from STARTED keeps that from running twice.
     */
    if (self != NULL && self->cfg.auto_connect_on_start &&
            net_iface_get_state(self) == NET_IFACE_STATE_STARTED) {
        net_iface_connect(self);
    }

    /*
     * stop() may have arrived before the main loop started. Skipping the loop
     * still runs the cleanup below, so the stack is not left allocated.
     */
    if (!s_ot_stop_requested) {
        esp_openthread_launch_mainloop();
    }

    /*
     * Clean up on this task, in the reverse order of initialization.
     * Glue and the netif go first, then the OpenThread stack, then eventfd.
     */
    thread_release_netif();
    (void)esp_openthread_deinit();
    thread_release_eventfd();
    ot_signal_task_exited();
    vTaskDelete(NULL);
}

/* Register eventfd and start the OpenThread task. Does not wait for attach. */
static esp_err_t thread_ops_start(net_iface_handle_t self)
{
    if (s_ot_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ot_stop_sem == NULL) {
        s_ot_stop_sem = xSemaphoreCreateBinary();
        if (s_ot_stop_sem == NULL) {
            return ESP_ERR_NO_MEM;
        }
    } else {
        /* Drop a signal left by a task that already exited. */
        (void)xSemaphoreTake(s_ot_stop_sem, 0);
    }
    s_ot_stop_requested = false;
    s_ot_task_published = false;

    /* netif, ot task queue, radio driver, and border router can each use one fd. */
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 4,
    };
    esp_err_t ret = esp_vfs_eventfd_register(&eventfd_config);
    if (ret == ESP_OK) {
        s_eventfd_owned = true;
    } else if (ret != ESP_ERR_INVALID_STATE) {
        vSemaphoreDelete(s_ot_stop_sem);
        s_ot_stop_sem = NULL;
        ESP_LOGE(TAG, "Failed to register eventfd: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_register(OPENTHREAD_EVENT, OPENTHREAD_EVENT_SET_DNS_SERVER,
                               on_thread_dns_server, NULL);

    uint32_t stack_size = CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE;
    if (self->cfg.thread.task_stack_size > 0) {
        stack_size = (uint32_t)self->cfg.thread.task_stack_size;
    }

    if (xTaskCreate(ot_task_worker, "ot_br_main", stack_size, self, 5, &s_ot_task_handle) != pdPASS) {
        esp_event_handler_unregister(OPENTHREAD_EVENT, OPENTHREAD_EVENT_SET_DNS_SERVER,
                                     on_thread_dns_server);
        thread_release_resources();
        vSemaphoreDelete(s_ot_stop_sem);
        s_ot_stop_sem = NULL;
        s_ot_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create OpenThread task");
        return ESP_FAIL;
    }
    s_ot_task_published = true;
    return ESP_OK;
}

/* Join the Thread network. Returns as soon as auto-start has been requested. */
static esp_err_t thread_ops_connect(net_iface_handle_t self)
{
    (void)self;
    if (esp_openthread_get_instance() == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_openthread_lock_acquire(portMAX_DELAY);
    esp_err_t err = thread_auto_start_locked();
    esp_openthread_lock_release();
    return err;
}

/* Leave the Thread network. The OpenThread task keeps running. */
static esp_err_t thread_ops_disconnect(net_iface_handle_t self)
{
    (void)self;
    otInstance *instance = esp_openthread_get_instance();
    if (instance == NULL) {
        return ESP_OK;
    }

    esp_openthread_lock_acquire(portMAX_DELAY);
    otError error = otThreadSetEnabled(instance, false);
    esp_openthread_lock_release();
    if (error != OT_ERROR_NONE) {
        ESP_LOGW(TAG, "otThreadSetEnabled(false) returned %d", (int)error);
    }
    return ESP_OK;
}

/*
 * Ask the OpenThread main loop to exit, then wait until ot_task_worker has
 * deinitialized the stack and deleted itself.
 */
static esp_err_t thread_ops_stop(net_iface_handle_t self)
{
    (void)self;
    esp_event_handler_unregister(OPENTHREAD_EVENT, OPENTHREAD_EVENT_SET_DNS_SERVER,
                                 on_thread_dns_server);
    bool task_exited = true;
    if (s_ot_stop_sem != NULL) {
        s_ot_stop_requested = true;
        otInstance *instance = esp_openthread_get_instance();
        if (instance != NULL) {
            esp_openthread_lock_acquire(portMAX_DELAY);
            (void)otThreadSetEnabled(instance, false);
            (void)otIp6SetEnabled(instance, false);
            esp_openthread_lock_release();
            (void)esp_openthread_mainloop_exit();
        }
        if (xSemaphoreTake(s_ot_stop_sem, pdMS_TO_TICKS(5000)) == pdTRUE) {
            vSemaphoreDelete(s_ot_stop_sem);
            s_ot_stop_sem = NULL;
        } else {
            /* The worker is still using the netif. Do not free it from here. */
            task_exited = false;
            ESP_LOGW(TAG, "Timed out waiting for OpenThread task to exit");
        }
    }
    /* The worker frees its own objects before signaling. This covers a start that never created the task. */
    if (task_exited) {
        thread_release_resources();
    }
    return ESP_OK;
}

/* Driver callbacks used by net_iface_init() and net_thread_create(). */
const net_iface_ops_t g_net_iface_thread_ops = {
    .name = "thread",
    .init = NULL,
    .deinit = NULL,
    .start = thread_ops_start,
    .stop = thread_ops_stop,
    .connect = thread_ops_connect,
    .disconnect = thread_ops_disconnect,
};

esp_err_t net_thread_create(const net_thread_params_t *params, net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    net_iface_config_t cfg = { 0 };
    cfg.type = NET_IFACE_TYPE_THREAD;
    cfg.desc = NET_CONNECT_NETIF_DESC_THREAD;
    cfg.route_prio = NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO;
#if CONFIG_NET_CONNECT_THREAD_REQUIRED
    cfg.required = true;
#else
    cfg.required = false;
#endif
    cfg.auto_connect_on_start = true;
    cfg.ip.enable_ipv6 = true;
    cfg.thread.task_stack_size = CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE;

    if (params != NULL) {
        if (params->task_stack_size > 0) {
            cfg.thread.task_stack_size = params->task_stack_size;
        }
        if (params->route_prio >= 0) {
            cfg.route_prio = params->route_prio;
        }
        cfg.required = params->required;
        cfg.auto_connect_on_start = params->auto_connect_on_start;
        if (params->ip != NULL) {
            cfg.ip = *params->ip;
        }
    }

    return net_iface_init_with_ops(&cfg, &g_net_iface_thread_ops, out_handle);
}
