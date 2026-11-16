/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file net_iface_events_example.c
 * @brief Fully event-driven net_iface lifecycle using NET_IFACE_EVENT callbacks.
 *
 * This example never calls net_connect() or net_iface_wait*().
 * app_main() registers the NET_IFACE_EVENT callback, starts the worker task,
 * and returns immediately.
 *
 * Part 1 (Deferred Provisioning):
 *   Starts Wi-Fi with auto_connect_on_start = false and an empty SSID.
 *   When NET_IFACE_EVENT_STARTED arrives, the callback applies the Kconfig
 *   credentials with net_wifi_sta_set_credentials() and calls net_iface_connect().
 *   When NET_IFACE_EVENT_READY arrives, the callback tells the worker task to
 *   start network activity, then tear down from the worker task.
 *
 * Part 2 (Failure Recovery and Pause on Disconnect):
 *   Starts Wi-Fi with an unreachable SSID ("invalid_ap_demo") and max_retry = 1.
 *   When NET_IFACE_EVENT_FAILED arrives, the callback inspects data->err,
 *   updates credentials to the real AP via net_wifi_sta_set_credentials(),
 *   and calls net_iface_connect() to recover. Once READY, the worker begins
 *   periodic network checks, drops the link with net_iface_disconnect() to
 *   trigger NET_IFACE_EVENT_DISCONNECTED, pauses network activity on the
 *   resulting event, and then stops and deinits the interface.
 */

#include <stdio.h>
#include <stdbool.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "net_connect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "net_iface_events";

/* Maximum time the worker waits for the next lifecycle event in a demo part. */
#define EVENT_WAIT_TIMEOUT_MS 25000

/* Deliberately unreachable SSID used in Part 2 to trigger NET_IFACE_EVENT_FAILED. */
#define DEMO_INVALID_WIFI_SSID "invalid_ap_demo"
#define DEMO_INVALID_WIFI_PASS "wrong_password"

typedef enum {
    PART_1_DEFERRED_PROVISIONING = 1,
    PART_2_FAILURE_RECOVERY      = 2,
} example_part_t;

typedef enum {
    APP_CMD_START_WORK = 0, /*!< Interface reached READY; start or resume network I/O */
    APP_CMD_PAUSE_WORK,     /*!< Interface lost IP or disconnected; pause network I/O */
    APP_CMD_FAILED,         /*!< Unrecovered failure; abort current part */
} app_cmd_id_t;

typedef struct {
    app_cmd_id_t       id;
    net_iface_handle_t iface;
    esp_err_t          err;
} app_cmd_t;

/* Commands from the event callback to the worker. The callback must not block. */
static QueueHandle_t s_cmd_queue;
/* Start time of the current part, so logs can print "+N ms". */
static int64_t s_part_start_us;
/* Which part is running. The callback uses this to decide what to do. */
static volatile example_part_t s_current_part = PART_1_DEFERRED_PROVISIONING;
/* Part 2 recovers from the bad SSID only once. A second failure ends the part. */
static volatile bool s_part2_recovered = false;

/* Milliseconds since the given timestamp. */
static int64_t elapsed_ms(int64_t since_us)
{
    return (esp_timer_get_time() - since_us) / 1000;
}

/* Word used in the log for a net_iface state. */
static const char *state_name(net_iface_state_t state)
{
    switch (state) {
    case NET_IFACE_STATE_UNINIT:
        return "UNINIT";
    case NET_IFACE_STATE_INIT:
        return "INIT";
    case NET_IFACE_STATE_STARTING:
        return "STARTING";
    case NET_IFACE_STATE_STARTED:
        return "STARTED";
    case NET_IFACE_STATE_CONNECTING:
        return "CONNECTING";
    case NET_IFACE_STATE_CONNECTED:
        return "CONNECTED";
    case NET_IFACE_STATE_READY:
        return "READY";
    case NET_IFACE_STATE_STOPPING:
        return "STOPPING";
    case NET_IFACE_STATE_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

/* Word used in the log for an interface type. */
static const char *type_name(net_iface_type_t type)
{
    switch (type) {
    case NET_IFACE_TYPE_WIFI_STA:
        return "Wi-Fi";
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

/* Word used in the log for a NET_IFACE_EVENT. */
static const char *event_name(net_iface_event_id_t id)
{
    switch (id) {
    case NET_IFACE_EVENT_STARTED:
        return "STARTED";
    case NET_IFACE_EVENT_CONNECTED:
        return "CONNECTED";
    case NET_IFACE_EVENT_READY:
        return "READY";
    case NET_IFACE_EVENT_LOST_IP:
        return "LOST_IP";
    case NET_IFACE_EVENT_DISCONNECTED:
        return "DISCONNECTED";
    case NET_IFACE_EVENT_STOPPED:
        return "STOPPED";
    case NET_IFACE_EVENT_FAILED:
        return "FAILED";
    default:
        return "UNKNOWN";
    }
}

/* Hand a command to the worker. Called from the event callback. */
static void send_worker_cmd(app_cmd_id_t id, net_iface_handle_t iface, esp_err_t err)
{
    app_cmd_t cmd = {
        .id = id,
        .iface = iface,
        .err = err,
    };
    /* Zero timeout: if the queue is full, drop the command. Never stall the event loop. */
    if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Worker command queue full, dropped cmd %d", (int)id);
    }
}

/*
 * Runs on the default event loop task, so it must return quickly.
 * It may set credentials, call net_iface_connect(), read the IP, and post a
 * command. It must not call net_iface_stop(), net_iface_wait*(), or any
 * socket function. Those run on the worker.
 */
static void on_net_iface_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;
    net_iface_event_id_t id = (net_iface_event_id_t)event_id;

    if (data == NULL) {
        return;
    }

    /* One line for every event, so the serial log shows the whole lifecycle. */
    ESP_LOGI(TAG, "+%lld ms  [event] %s -> NET_IFACE_EVENT_%s (state=%s)",
             (long long)elapsed_ms(s_part_start_us),
             type_name(data->type), event_name(id), state_name(data->state));

    switch (id) {
    case NET_IFACE_EVENT_STARTED:
        /*
         * Part 1 started with an empty name, so the radio is up but not joined.
         * Store the SSID from Kconfig and connect now.
         * Part 2 already has a name and connects on its own, so nothing to do.
         */
        if (s_current_part == PART_1_DEFERRED_PROVISIONING) {
            ESP_LOGI(TAG, "+%lld ms  [event] Driver is STARTED; provisioning credentials for '%s' and connecting",
                     (long long)elapsed_ms(s_part_start_us), CONFIG_NET_CONNECT_WIFI_SSID);
            ESP_ERROR_CHECK(net_wifi_sta_set_credentials(data->iface,
                                                         CONFIG_NET_CONNECT_WIFI_SSID,
                                                         CONFIG_NET_CONNECT_WIFI_PASSWORD));
            ESP_ERROR_CHECK(net_iface_connect(data->iface));
        }
        break;

    case NET_IFACE_EVENT_CONNECTED:
        /* The AP link is up. The address arrives in the next event. */
        ESP_LOGI(TAG, "+%lld ms  [event] L2 link is up; waiting for IP address...",
                 (long long)elapsed_ms(s_part_start_us));
        break;

    case NET_IFACE_EVENT_READY: {
        /* Print the address, then let the worker start its network checks. */
        esp_netif_ip_info_t ip_info = { 0 };
        if (data->netif != NULL && esp_netif_get_ip_info(data->netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "+%lld ms  [event] %s is READY with IP " IPSTR "; notifying worker task",
                     (long long)elapsed_ms(s_part_start_us),
                     type_name(data->type), IP2STR(&ip_info.ip));
        }
        send_worker_cmd(APP_CMD_START_WORK, data->iface, ESP_OK);
        break;
    }

    case NET_IFACE_EVENT_LOST_IP:
    case NET_IFACE_EVENT_DISCONNECTED:
        /* The network is no longer usable. Tell the worker to stop using it now. */
        ESP_LOGI(TAG, "+%lld ms  [event] Link/IP lost (%s); telling worker task to pause network I/O",
                 (long long)elapsed_ms(s_part_start_us), event_name(id));
        send_worker_cmd(APP_CMD_PAUSE_WORK, data->iface, ESP_OK);
        break;

    case NET_IFACE_EVENT_FAILED:
        /* data->err is the reason on this event. last_error is the same value saved on the handle. */
        ESP_LOGW(TAG, "+%lld ms  [event] %s FAILED (data->err=%s, last_error=%s)",
                 (long long)elapsed_ms(s_part_start_us),
                 type_name(data->type),
                 esp_err_to_name(data->err),
                 esp_err_to_name(net_iface_get_last_error(data->iface)));

        /*
         * Part 2's first failure is the bad SSID, and it is expected.
         * Switch to the real AP and try once more.
         * A later failure, or any failure in Part 1, tells the worker to stop this part.
         */
        if (s_current_part == PART_2_FAILURE_RECOVERY && !s_part2_recovered) {
            s_part2_recovered = true;
            ESP_LOGI(TAG, "+%lld ms  [event] Recovering from FAILED: switching to '%s' and calling net_iface_connect()",
                     (long long)elapsed_ms(s_part_start_us), CONFIG_NET_CONNECT_WIFI_SSID);
            ESP_ERROR_CHECK(net_wifi_sta_set_credentials(data->iface,
                                                         CONFIG_NET_CONNECT_WIFI_SSID,
                                                         CONFIG_NET_CONNECT_WIFI_PASSWORD));
            ESP_ERROR_CHECK(net_iface_connect(data->iface));
        } else {
            send_worker_cmd(APP_CMD_FAILED, data->iface, data->err);
        }
        break;

    case NET_IFACE_EVENT_STOPPED:
        /* The driver and the netif are gone. The worker already asked for this. */
        ESP_LOGI(TAG, "+%lld ms  [event] %s STOPPED (netif destroyed, state=%s)",
                 (long long)elapsed_ms(s_part_start_us),
                 type_name(data->type), state_name(data->state));
        break;

    default:
        break;
    }
}

/* One name lookup. getaddrinfo() blocks, so this stays on the worker. */
static void do_one_network_check(net_iface_handle_t h, int step, int total_steps)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    /* Time the lookup itself, separate from the time since the part started. */
    int64_t op_us = esp_timer_get_time();
    int rc = getaddrinfo("example.com", NULL, &hints, &res);

    if (rc == 0 && res != NULL) {
        char host[INET_ADDRSTRLEN] = { 0 };
        const struct sockaddr_in *addr = (const struct sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &addr->sin_addr, host, sizeof(host));
        ESP_LOGI(TAG, "+%lld ms  [worker] step %d/%d (%s=%s): getaddrinfo(example.com) -> %s (blocked %lld ms)",
                 (long long)elapsed_ms(s_part_start_us), step, total_steps,
                 type_name(net_iface_get_type(h)), state_name(net_iface_get_state(h)),
                 host, (long long)elapsed_ms(op_us));
        freeaddrinfo(res);
    } else {
        ESP_LOGW(TAG, "+%lld ms  [worker] step %d/%d (%s=%s): getaddrinfo(example.com) failed (%d, blocked %lld ms)",
                 (long long)elapsed_ms(s_part_start_us), step, total_steps,
                 type_name(net_iface_get_type(h)), state_name(net_iface_get_state(h)),
                 rc, (long long)elapsed_ms(op_us));
    }
}

/*
 * Part 1: start with no SSID. When STARTED arrives, the callback fills in the
 * real credentials and connects. This task waits, uses the network, then shuts down.
 */
static void run_part1_deferred_provisioning(void)
{
    net_iface_handle_t wifi = NULL;
    app_cmd_t cmd = { 0 };

    ESP_LOGI(TAG, "\n" LOG_ANSI_COLOR_BOLD(LOG_ANSI_COLOR_CYAN)
             "=============== Part 1: deferred provisioning via NET_IFACE_EVENT_STARTED ===============");
    /* Drop commands left by an earlier shutdown so they cannot wake this part. */
    xQueueReset(s_cmd_queue);
    s_current_part = PART_1_DEFERRED_PROVISIONING;
    s_part_start_us = esp_timer_get_time();

    /*
     * Empty name and auto-connect off: the radio starts, but it does not join.
     * -1 keeps the retry count from Kconfig and the default route priority.
     */
    const net_wifi_sta_params_t params = {
        .ssid = "",
        .password = "",
        .max_retry = -1,
        .route_prio = -1,
        .auto_connect_on_start = false,
    };
    ESP_ERROR_CHECK(net_wifi_sta_create(&params, &wifi));
    /* Returns at once. The STARTED event does the provisioning. */
    ESP_ERROR_CHECK(net_iface_start(wifi));
    ESP_LOGI(TAG, "+%lld ms  [worker] net_iface_start(wifi) returned immediately (state=%s)",
             (long long)elapsed_ms(s_part_start_us), state_name(net_iface_get_state(wifi)));

    /* Sleep until the callback says the address is ready, or the attempt failed. */
    if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)) == pdTRUE &&
            cmd.id == APP_CMD_START_WORK) {
        for (int step = 1; step <= 3; step++) {
            do_one_network_check(wifi, step, 3);
            /* One second between lookups, but wake at once if the link drops. */
            if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE &&
                    cmd.id == APP_CMD_PAUSE_WORK) {
                ESP_LOGW(TAG, "+%lld ms  [worker] Paused network work early (state=%s)",
                         (long long)elapsed_ms(s_part_start_us), state_name(net_iface_get_state(wifi)));
                break;
            }
        }
    } else {
        ESP_LOGW(TAG, "+%lld ms  [worker] Part 1 did not reach READY (state=%s)",
                 (long long)elapsed_ms(s_part_start_us), state_name(net_iface_get_state(wifi)));
    }

    /* Stop and free the handle here. These calls are not allowed in the event callback. */
    ESP_LOGI(TAG, "+%lld ms  [worker] Stopping and deinitializing Wi-Fi from worker task",
             (long long)elapsed_ms(s_part_start_us));
    ESP_ERROR_CHECK(net_iface_stop(wifi));
    ESP_ERROR_CHECK(net_iface_deinit(wifi));
}

/*
 * Part 2: join a name that does not exist, recover when FAILED arrives, use the
 * network, then drop the link and pause before shutting down.
 */
static void run_part2_failure_recovery_and_pause(void)
{
    net_iface_handle_t wifi = NULL;
    app_cmd_t cmd = { 0 };

    ESP_LOGI(TAG, "\n" LOG_ANSI_COLOR_BOLD(LOG_ANSI_COLOR_CYAN)
             "=============== Part 2: FAILED recovery and pause on DISCONNECTED ===============");
    /* Part 1's stop can leave a disconnect command in the queue. Clear it. */
    xQueueReset(s_cmd_queue);
    s_current_part = PART_2_FAILURE_RECOVERY;
    s_part2_recovered = false;
    s_part_start_us = esp_timer_get_time();

    /*
     * Auto-connect is on, so the station joins this name by itself.
     * max_retry = 1 means two attempts, then FAILED. The later recovery join
     * keeps this same limit. -1 would use the longer Kconfig retry count.
     */
    const net_wifi_sta_params_t bad_params = {
        .ssid = DEMO_INVALID_WIFI_SSID,
        .password = DEMO_INVALID_WIFI_PASS,
        .max_retry = 1,
        .route_prio = -1,
        .auto_connect_on_start = true,
    };
    ESP_ERROR_CHECK(net_wifi_sta_create(&bad_params, &wifi));
    ESP_ERROR_CHECK(net_iface_start(wifi));
    ESP_LOGI(TAG, "+%lld ms  [worker] Started Wi-Fi with bad SSID '%s' (max_retry=1, state=%s)",
             (long long)elapsed_ms(s_part_start_us),
             DEMO_INVALID_WIFI_SSID, state_name(net_iface_get_state(wifi)));

    /*
     * This task does not poll and does not call net_iface_wait().
     * The callback sees FAILED, stores the real SSID, and connects again.
     * READY then posts APP_CMD_START_WORK, which wakes the wait below.
     */
    if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(EVENT_WAIT_TIMEOUT_MS)) == pdTRUE &&
            cmd.id == APP_CMD_START_WORK) {
        for (int step = 1; step <= 4; step++) {
            do_one_network_check(wifi, step, 4);

            /* After two lookups, drop the link. The driver stays started. */
            if (step == 2) {
                ESP_LOGI(TAG, "+%lld ms  [worker] Calling net_iface_disconnect() to simulate link loss...",
                         (long long)elapsed_ms(s_part_start_us));
                ESP_ERROR_CHECK(net_iface_disconnect(wifi));
            }

            /* DISCONNECTED posts a pause command. Leave the loop when it arrives. */
            if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE &&
                    cmd.id == APP_CMD_PAUSE_WORK) {
                ESP_LOGI(TAG, "+%lld ms  [worker] Received pause command; network activity paused (state=%s)",
                         (long long)elapsed_ms(s_part_start_us), state_name(net_iface_get_state(wifi)));
                break;
            }
        }
    } else {
        ESP_LOGW(TAG, "+%lld ms  [worker] Part 2 did not reach READY after recovery (state=%s)",
                 (long long)elapsed_ms(s_part_start_us), state_name(net_iface_get_state(wifi)));
    }

    /* Same rule as Part 1: stop and free the handle on this task, not in the callback. */
    ESP_LOGI(TAG, "+%lld ms  [worker] Stopping and deinitializing Wi-Fi from worker task",
             (long long)elapsed_ms(s_part_start_us));
    ESP_ERROR_CHECK(net_iface_stop(wifi));
    ESP_ERROR_CHECK(net_iface_deinit(wifi));
}

/* Runs both parts, then stops listening and deletes itself. */
static void app_worker_task(void *arg)
{
    (void)arg;
    run_part1_deferred_provisioning();
    run_part2_failure_recovery_and_pause();

    /* Safe to unregister here. app_main has already returned. */
    ESP_ERROR_CHECK(net_iface_unregister_event_handler(on_net_iface_event));
    ESP_LOGI(TAG, "Example finished");
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* net_iface needs the network stack and the default event loop. */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Eight slots is enough. Sends from the callback use a zero timeout. */
    s_cmd_queue = xQueueCreate(8, sizeof(app_cmd_t));
    ESP_ERROR_CHECK(s_cmd_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    /* Register before the worker starts, so the first STARTED event is not missed. */
    ESP_ERROR_CHECK(net_iface_register_event_handler(on_net_iface_event, NULL));

    ESP_LOGI(TAG, "Target Wi-Fi SSID from Kconfig: %s", CONFIG_NET_CONNECT_WIFI_SSID);
    BaseType_t created = xTaskCreate(app_worker_task, "app_worker", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    /* Setup is done. The callback and the worker run the rest of the example. */
    ESP_LOGI(TAG, "app_main() setup complete; returning now while events and worker task run");
}
