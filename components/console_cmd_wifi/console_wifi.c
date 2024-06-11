/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip6.h"
#include "lwip/opt.h"
#include "esp_wifi.h"
#include "console_wifi.h"


#define DEFAULT_SCAN_LIST_SIZE 10

#if CONFIG_WIFI_CMD_AUTO_REGISTRATION
/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "console_cmd_wifi",
    .plugin_regd_fn = &console_cmd_wifi_register
};
#endif

typedef struct wifi_op_t {
    char *name;
    esp_err_t (*operation)(struct wifi_op_t *self, int argc, char *argv[]);
    int arg_cnt;
    int start_index;
    char *help;
} wifi_op_t;

static esp_err_t wifi_help_op(wifi_op_t *self, int argc, char *argv[]);
static esp_err_t wifi_show_op(wifi_op_t *self, int argc, char *argv[]);
static esp_err_t wifi_sta_join_op(wifi_op_t *self, int argc, char *argv[]);
static esp_err_t wifi_sta_leave_op(wifi_op_t *self, int argc, char *argv[]);

static const char *TAG = "console_wifi";

#define JOIN_TIMEOUT_MS (10000)

static EventGroupHandle_t wifi_event_group;
static const int STA_STARTED_BIT = BIT0;
static const int CONNECTED_BIT = BIT1;

static wifi_op_t cmd_list[] = {
    {.name = "help", .operation = wifi_help_op, .arg_cnt = 2, .start_index = 1, .help = "wifi help: Prints the help text for all wifi commands"},
    {.name = "show", .operation = wifi_show_op, .arg_cnt = 3, .start_index = 1, .help = "wifi show network/sta: Scans and displays all available wifi APs./ Shows the details of wifi station."},
    {.name = "join",  .operation = wifi_sta_join_op,  .arg_cnt = 5, .start_index = 2, .help = "wifi sta join <network ssid> <password>: Station joins the given wifi network."},
    {.name = "join",  .operation = wifi_sta_join_op,  .arg_cnt = 4, .start_index = 2, .help = "wifi sta join <network ssid>: Station joins the given unsecured wifi network."},
    {.name = "join",  .operation = wifi_sta_join_op,  .arg_cnt = 3, .start_index = 2, .help = "wifi sta join: Station joins the pre-configured wifi network."},
    {.name = "leave", .operation = wifi_sta_leave_op,  .arg_cnt = 3, .start_index = 2, .help = "wifi sta leave: Station leaves the wifi network."},
};

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(wifi_event_group, STA_STARTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

static esp_err_t wifi_help_op(wifi_op_t *self, int argc, char *argv[])
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);

    for (int i = 0; i < cmd_count; i++) {
        if ((cmd_list[i].help != NULL) && (strlen(cmd_list[i].help) != 0)) {
            printf(" %s\n", cmd_list[i].help);
        }
    }

    return ESP_OK;
}

uint8_t wifi_connection_status = 0;

void wifi_init(void)
{
    static bool init_flag = false;
    if (init_flag) {
        return;
    }
    wifi_event_group = xEventGroupCreate();

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_wifi_start());

    int bits = xEventGroupWaitBits(wifi_event_group, STA_STARTED_BIT,
                                   pdFALSE, pdTRUE, JOIN_TIMEOUT_MS / portTICK_PERIOD_MS);

    if ((bits & STA_STARTED_BIT) == 0) {
        ESP_LOGE(TAG, "Error: Wifi Connection timed out");
        return;
    }

    init_flag = true;
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;

    memset(ap_info, 0, sizeof(ap_info));

    wifi_init();

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    ESP_LOGI(TAG, "Showing Wifi networks");
    ESP_LOGI(TAG, "*********************");
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "RSSI: %d\tChannel: %d\tSSID: %s", ap_info[i].rssi, ap_info[i].primary, ap_info[i].ssid);
    }
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
}

static esp_err_t wifi_show_op(wifi_op_t *self, int argc, char *argv[])
{
    if (!strcmp("network", argv[self->start_index + 1])) {
        wifi_scan();
        return ESP_OK;
    }

    if (!strcmp("sta", argv[self->start_index + 1])) {
        {
            wifi_config_t wifi_config = { 0 };

            wifi_init();
            ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &wifi_config));

            ESP_LOGI(TAG, "Showing Joind AP details:");
            ESP_LOGI(TAG, "*************************");
            ESP_LOGI(TAG, "SSID: %s", wifi_config.sta.ssid);
            ESP_LOGI(TAG, "Channel: %d", wifi_config.sta.channel);
            ESP_LOGI(TAG, "bssid: %02x:%02x:%02x:%02x:%02x:%02x", wifi_config.sta.bssid[0],
                     wifi_config.sta.bssid[1], wifi_config.sta.bssid[2], wifi_config.sta.bssid[3],
                     wifi_config.sta.bssid[4], wifi_config.sta.bssid[5]);
        }
        return ESP_OK;
    }

    return ESP_OK;
}

static esp_err_t wifi_sta_join_op(wifi_op_t *self, int argc, char *argv[])
{
    wifi_config_t wifi_config = { 0 };

    if (strcmp("sta", argv[self->start_index - 1])) {
        ESP_LOGE(TAG, "Error: Invalid command\n");
        ESP_LOGE(TAG, "%s\n", self->help);
        return ESP_FAIL;
    }

    if (self->arg_cnt == 3) {
        strlcpy((char *) wifi_config.sta.ssid, CONFIG_WIFI_CMD_NETWORK_SSID, sizeof(wifi_config.sta.ssid));
        strlcpy((char *) wifi_config.sta.password, CONFIG_WIFI_CMD_NETWORK_PASSWORD, sizeof(wifi_config.sta.password));
    } else {
        strlcpy((char *) wifi_config.sta.ssid, argv[self->start_index + 1], sizeof(wifi_config.sta.ssid));
    }

    if (self->arg_cnt == 5) {
        strlcpy((char *) wifi_config.sta.password, argv[self->start_index + 2], sizeof(wifi_config.sta.password));
    }

    wifi_init();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();

    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                   pdFALSE, pdTRUE, JOIN_TIMEOUT_MS / portTICK_PERIOD_MS);

    if ((bits & CONNECTED_BIT) == 0) {
        ESP_LOGE(TAG, "Error: Wifi Connection timed out");
        return ESP_OK;
    }

    return ESP_OK;
}

static esp_err_t wifi_sta_leave_op(wifi_op_t *self, int argc, char *argv[])
{
    wifi_config_t wifi_config = { 0 };

    if (strcmp("sta", argv[self->start_index - 1])) {
        ESP_LOGE(TAG, "Error: Invalid command\n");
        ESP_LOGE(TAG, "%s\n", self->help);
        return ESP_FAIL;
    }

    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    return ESP_OK;
}

/* handle 'wifi' command */
static esp_err_t do_cmd_wifi(int argc, char **argv)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    wifi_op_t cmd;

    for (int i = 0; i < cmd_count; i++) {
        cmd = cmd_list[i];

        if (argc < cmd.start_index + 1) {
            continue;
        }

        if (!strcmp(cmd.name, argv[cmd.start_index])) {
            /* Get interface for eligible commands */
            if (cmd.arg_cnt == argc) {
                if (cmd.operation != NULL) {
                    if (cmd.operation(&cmd, argc, argv) != ESP_OK) {
                        ESP_LOGE(TAG, "Usage:\n%s", cmd.help);
                        return 0;
                    }
                }
                return ESP_OK;
            }
        }
    }

    ESP_LOGE(TAG, "Command not available");

    return ESP_OK;
}

/**
 * @brief Registers the wifi command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_wifi_register(void)
{
    esp_err_t ret = ESP_OK;
    esp_console_cmd_t command = {
        .command = "wifi",
        .help = "Command for wifi configuration and monitoring\n For more info run 'wifi help'",
        .func = &do_cmd_wifi
    };

    ret = esp_console_cmd_register(&command);
    if (ret) {
        ESP_LOGE(TAG, "Unable to register wifi");
    }

    return ret;
}
