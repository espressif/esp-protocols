/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Console commands for the Wi-Fi station.
 * "wifi_connect" stores the name and password, then starts joining.
 * "wifi_disconnect" drops the link and leaves the radio on.
 */

#include <stdio.h>
#include <string.h>
#include "net_connect.h"
#include "net_iface_priv.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

static const char *TAG = "net_connect_console";

/* Arguments for the wifi_connect command. */
typedef struct {
    struct arg_str *ssid;      /* Network name. Required. */
    struct arg_str *password;  /* Password. Omitted means an open network. */
    struct arg_int *channel;   /* Optional channel. 0 means any channel. */
    struct arg_end *end;       /* Collects parse errors. */
} wifi_connect_args_t;

static wifi_connect_args_t connect_args;

/* Find the station handle, or make an idle one that waits for this command. */
static net_iface_handle_t console_get_wifi(void)
{
    net_iface_handle_t h = net_iface_get_by_type(NET_IFACE_TYPE_WIFI_STA);
    if (h != NULL) {
        return h;
    }

    net_wifi_sta_params_t params = {
        .ssid = "",
        .password = NULL,
        .max_retry = -1,
        .route_prio = -1,
        .required = false,
        .auto_connect_on_start = false,
    };
    if (net_wifi_sta_create(&params, &h) != ESP_OK) {
        return NULL;
    }
    return h;
}

/* start() returns while the radio is still coming up. Wait until that work finishes. */
static esp_err_t console_wait_until_started(net_iface_handle_t h)
{
    if (net_iface_get_state(h) != NET_IFACE_STATE_STARTING) {
        return ESP_OK;
    }
    EventBits_t bits = xEventGroupWaitBits(h->state_events, NET_IFACE_BIT_STARTUP_DONE,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    if ((bits & NET_IFACE_BIT_STARTUP_DONE) == 0) {
        return ESP_ERR_TIMEOUT;
    }
    if (net_iface_get_state(h) == NET_IFACE_STATE_FAILED) {
        return net_iface_get_last_error(h);
    }
    return ESP_OK;
}

/* Make sure the radio is on. An idle station with no name stays started. */
static esp_err_t console_ensure_started(net_iface_handle_t h)
{
    net_iface_state_t state = net_iface_get_state(h);
    if (state != NET_IFACE_STATE_INIT && state != NET_IFACE_STATE_FAILED) {
        return console_wait_until_started(h);
    }
    esp_err_t err = net_iface_start(h);
    if (err != ESP_OK) {
        return err;
    }
    return console_wait_until_started(h);
}

/* Join the network named on the command line. */
static int cmd_do_wifi_connect(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&connect_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, connect_args.end, argv[0]);
        return 1;
    }

    net_iface_handle_t h = console_get_wifi();
    if (h == NULL) {
        ESP_LOGE(TAG, "No Wi-Fi station handle");
        return 1;
    }
    if (console_ensure_started(h) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi");
        return 1;
    }

    net_iface_state_t state = net_iface_get_state(h);
    if (state == NET_IFACE_STATE_CONNECTING ||
            state == NET_IFACE_STATE_CONNECTED ||
            state == NET_IFACE_STATE_READY) {
        net_iface_disconnect(h);
    }

    const char *ssid = connect_args.ssid->sval[0];
    const char *pass = NULL;
    if (connect_args.password->count > 0) {
        pass = connect_args.password->sval[0];
    }
    esp_err_t ret = net_wifi_sta_set_credentials(h, ssid, pass);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store Wi-Fi credentials: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = net_iface_connect(h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        return 1;
    }

    /*
     * The channel is not part of the stored station config, so apply it
     * after connect starts. Channel 0 already means "any channel".
     */
    if (connect_args.channel->count > 0) {
        uint8_t channel = (uint8_t)connect_args.channel->ival[0];
        if (channel != 0) {
            esp_err_t ch_err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
            if (ch_err != ESP_OK) {
                ESP_LOGW(TAG, "esp_wifi_set_channel(%u) returned %s", channel, esp_err_to_name(ch_err));
            }
        }
    }
    return 0;
}

/* Drop the Wi-Fi link. The radio stays on for a later wifi_connect. */
static int cmd_do_wifi_disconnect(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    net_iface_handle_t h = net_iface_get_by_type(NET_IFACE_TYPE_WIFI_STA);
    if (h == NULL) {
        return 0;
    }
    net_iface_state_t state = net_iface_get_state(h);
    if (state == NET_IFACE_STATE_CONNECTING ||
            state == NET_IFACE_STATE_CONNECTED ||
            state == NET_IFACE_STATE_READY ||
            state == NET_IFACE_STATE_FAILED) {
        net_iface_disconnect(h);
    }
    return 0;
}

void net_register_wifi_connect_commands(void)
{
    ESP_LOGI(TAG, "Registering WiFi connect commands.");

    net_iface_handle_t h = console_get_wifi();
    if (h == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi station");
        return;
    }
    esp_err_t ret = console_ensure_started(h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return;
    }

    connect_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    connect_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    connect_args.channel = arg_int0("n", "channel", "<channel>", "channel of AP");
    connect_args.end = arg_end(2);
    const esp_console_cmd_t wifi_connect_cmd = {
        .command = "wifi_connect",
        .help = "WiFi is station mode, join specified soft-AP",
        .hint = NULL,
        .func = &cmd_do_wifi_connect,
        .argtable = &connect_args
    };
    ret = esp_console_cmd_register(&wifi_connect_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register wifi_connect command: %s", esp_err_to_name(ret));
        return;
    }

    const esp_console_cmd_t wifi_disconnect_cmd = {
        .command = "wifi_disconnect",
        .help = "Do wifi disconnect",
        .hint = NULL,
        .func = &cmd_do_wifi_disconnect,
    };
    ret = esp_console_cmd_register(&wifi_disconnect_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register wifi_disconnect command: %s", esp_err_to_name(ret));
    }
}
