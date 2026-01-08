/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <string.h>
#include "net_connect.h"
#include "net_connect_private.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"


static const char *TAG = "net_connect_console";

typedef struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_int *channel;
    struct arg_end *end;
} wifi_connect_args_t;
static wifi_connect_args_t connect_args;

static int cmd_do_wifi_connect(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &connect_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, connect_args.end, argv[0]);
        return 1;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        },
    };
    if (connect_args.channel->count > 0) {
        wifi_config.sta.channel = (uint8_t)(connect_args.channel->ival[0]);
    }
    const char *ssid = connect_args.ssid->sval[0];
    const char *pass = NULL;
    if (connect_args.password->count > 0) {
        pass = connect_args.password->sval[0];
    }
    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }
    esp_err_t ret = net_connect_wifi_sta_do_connect(wifi_config, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int cmd_do_wifi_disconnect(int argc, char **argv)
{
    net_connect_wifi_sta_do_disconnect();
    return 0;
}

void net_register_wifi_connect_commands(void)
{
    ESP_LOGI(TAG, "Registering WiFi connect commands.");
    esp_err_t ret = net_connect_wifi_start();
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
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_connect_cmd));


    const esp_console_cmd_t wifi_disconnect_cmd = {
        .command = "wifi_disconnect",
        .help = "Do wifi disconnect",
        .hint = NULL,
        .func = &cmd_do_wifi_disconnect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_disconnect_cmd));
}
