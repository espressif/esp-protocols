/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file net_iface_example.c
 * @brief Start interfaces in the background, then wait when the app needs them.
 *
 * Everything runs in app_main(). There are no event callbacks and no extra tasks.
 *
 * Part 1 uses Wi-Fi alone:
 *   create -> net_iface_start() (returns immediately) -> the app does its own
 *   work while Wi-Fi connects -> net_iface_wait() -> use the network ->
 *   net_iface_stop() + net_iface_deinit().
 *
 * Part 2 does the same with Ethernet and Wi-Fi together. Both start() calls
 * return immediately. net_iface_wait_any() continues as soon as one interface
 * is ready. net_iface_wait_all() waits until both are ready.
 *
 * Only the calls that block print how long they held this task: the net_iface_wait*()
 * calls, and getaddrinfo().
 */

#include <stdio.h>
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

static const char *TAG = "net_iface_example";

/* How long to wait for an address before moving on. */
#define IFACE_WAIT_MS 15000

/* Shorter budget for the second wait, after one interface is already up. */
#define IFACE_WAIT_ALL_MS 5000

static int64_t elapsed_ms(int64_t since_us)
{
    return (esp_timer_get_time() - since_us) / 1000;
}

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

/* Pause and print the interface state. The network keeps connecting during these pauses. */
static void do_app_work(net_iface_handle_t h1, net_iface_handle_t h2, int steps)
{
    for (int step = 1; step <= steps; step++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        /* h2 is NULL when only one interface is running. */
        if (h2 == NULL) {
            ESP_LOGI(TAG, "[app] startup work %d/%d, %s state: %s",
                     step, steps, type_name(net_iface_get_type(h1)), state_name(net_iface_get_state(h1)));
        } else {
            ESP_LOGI(TAG, "[app] startup work %d/%d, %s: %s, %s: %s",
                     step, steps,
                     type_name(net_iface_get_type(h1)), state_name(net_iface_get_state(h1)),
                     type_name(net_iface_get_type(h2)), state_name(net_iface_get_state(h2)));
        }
    }
}

/* Use an interface that is already ready. A failed name lookup does not stop the example. */
static void do_network_activity(net_iface_handle_t h)
{
    esp_netif_ip_info_t ip_info = { 0 };
    esp_netif_t *netif = net_iface_get_netif(h);
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int64_t op_us;
    int rc;

    /* Print the address this interface received. */
    if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "%s address " IPSTR, type_name(net_iface_get_type(h)), IP2STR(&ip_info.ip));
    }

    /* Look up a name. This is the first real use of the network, and it can take a moment. */
    op_us = esp_timer_get_time();
    rc = getaddrinfo("example.com", NULL, &hints, &res);
    if (rc == 0 && res != NULL) {
        char host[INET_ADDRSTRLEN] = { 0 };
        const struct sockaddr_in *addr = (const struct sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &addr->sin_addr, host, sizeof(host));
        ESP_LOGI(TAG, "getaddrinfo(example.com) -> %s, blocked %lld ms", host, (long long)elapsed_ms(op_us));
        freeaddrinfo(res);
    } else {
        ESP_LOGW(TAG, "getaddrinfo(example.com) failed (%d), blocked %lld ms", rc, (long long)elapsed_ms(op_us));
    }

    /* Stay connected briefly so the link is up for more than one call. */
    ESP_LOGI(TAG, "[app] staying connected for 3 s");
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/* Bring up Wi-Fi, use it, then shut it down. */
static void run_single_interface_demo(void)
{
    net_iface_handle_t wifi = NULL;
    int64_t op_us;
    esp_err_t err;

    ESP_LOGI(TAG, "\n" LOG_ANSI_COLOR_BOLD(LOG_ANSI_COLOR_CYAN)
             "=============== Part 1: single interface (Wi-Fi) ===============");

    /* Create the handle. The radio is not on yet. NULL uses the SSID from Kconfig. */
    ESP_ERROR_CHECK(net_wifi_sta_create(NULL, &wifi));
    /* Start returns immediately. Wi-Fi connects on a background task. */
    ESP_ERROR_CHECK(net_iface_start(wifi));
    ESP_LOGI(TAG, "Wi-Fi state after start: %s", state_name(net_iface_get_state(wifi)));

    /* Do other work while Wi-Fi is still connecting. */
    do_app_work(wifi, NULL, 3);

    /* Block here until Wi-Fi has an address, or until the timeout. */
    op_us = esp_timer_get_time();
    err = net_iface_wait(wifi, IFACE_WAIT_MS);
    ESP_LOGI(TAG, "net_iface_wait(wifi) blocked %lld ms", (long long)elapsed_ms(op_us));
    ESP_LOGI(TAG, "net_iface_wait() returned %s, Wi-Fi state: %s",
             esp_err_to_name(err), state_name(net_iface_get_state(wifi)));

    /* Use the network only when the wait succeeded. */
    if (err == ESP_OK) {
        do_network_activity(wifi);
    } else {
        ESP_LOGW(TAG, "Wi-Fi did not become ready (%s). Continuing to Part 2.", esp_err_to_name(err));
    }

    /* Stop the driver, then free the handle. */
    ESP_ERROR_CHECK(net_iface_stop(wifi));
    ESP_ERROR_CHECK(net_iface_deinit(wifi));
}

/* Bring up Ethernet and Wi-Fi together, use them, then shut both down. */
static void run_multi_interface_demo(void)
{
    net_iface_handle_t eth = NULL;
    net_iface_handle_t wifi = NULL;
    net_iface_handle_t ifaces[2];
    net_iface_handle_t first_ready = NULL;
    int64_t op_us;
    esp_err_t err;

    ESP_LOGI(TAG, "\n" LOG_ANSI_COLOR_BOLD(LOG_ANSI_COLOR_CYAN)
             "=============== Part 2: Ethernet and Wi-Fi together ===============");

    /* Create both handles. Nothing is started yet. */
    ESP_ERROR_CHECK(net_eth_create(NULL, &eth));
    ESP_ERROR_CHECK(net_wifi_sta_create(NULL, &wifi));
    /* Each start returns immediately, so the two links come up together. */
    ESP_ERROR_CHECK(net_iface_start(eth));
    ESP_ERROR_CHECK(net_iface_start(wifi));
    ESP_LOGI(TAG, "After both starts: Ethernet %s, Wi-Fi %s",
             state_name(net_iface_get_state(eth)), state_name(net_iface_get_state(wifi)));

    /* Same application work, while both interfaces connect in the background. */
    do_app_work(eth, wifi, 3);

    ifaces[0] = eth;
    ifaces[1] = wifi;

    /* Continue as soon as either interface is ready. */
    op_us = esp_timer_get_time();
    err = net_iface_wait_any(ifaces, 2, IFACE_WAIT_MS, &first_ready);
    ESP_LOGI(TAG, "net_iface_wait_any() blocked %lld ms", (long long)elapsed_ms(op_us));
    if (err == ESP_OK && first_ready != NULL) {
        ESP_LOGI(TAG, "First ready interface: %s (%s)",
                 type_name(net_iface_get_type(first_ready)),
                 state_name(net_iface_get_state(first_ready)));
        /* Use whichever interface got there first. */
        do_network_activity(first_ready);
    } else {
        ESP_LOGW(TAG, "net_iface_wait_any() returned %s", esp_err_to_name(err));
    }

    /* Wait until both are ready. This often returns at once if the second one
     * finished during the step above. */
    op_us = esp_timer_get_time();
    err = net_iface_wait_all(ifaces, 2, IFACE_WAIT_ALL_MS);
    ESP_LOGI(TAG, "net_iface_wait_all() blocked %lld ms", (long long)elapsed_ms(op_us));
    ESP_LOGI(TAG, "net_iface_wait_all() returned %s, Ethernet: %s, Wi-Fi: %s",
             esp_err_to_name(err),
             state_name(net_iface_get_state(eth)), state_name(net_iface_get_state(wifi)));

    /* Stop and free both handles. */
    ESP_ERROR_CHECK(net_iface_stop(wifi));
    ESP_ERROR_CHECK(net_iface_stop(eth));
    ESP_ERROR_CHECK(net_iface_deinit(wifi));
    ESP_ERROR_CHECK(net_iface_deinit(eth));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    /* The network stack and the default event loop. net_iface needs both. */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Wi-Fi SSID from Kconfig: %s", CONFIG_NET_CONNECT_WIFI_SSID);
    /* Part 1 is Wi-Fi alone. Part 2 starts Ethernet and Wi-Fi together. */
    run_single_interface_demo();
    run_multi_interface_demo();
    ESP_LOGI(TAG, "Example finished");
}
