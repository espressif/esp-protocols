/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "iface_info.h"
#include "ppp_connect.h"
#include "driver/uart.h"

static const char *TAG = "ppp_connect_simple";

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    ESP_LOG_BUFFER_HEXDUMP("ppp_connect_tx", buffer, len, ESP_LOG_VERBOSE);
    uart_write_bytes(UART_NUM_1, buffer, len);
    return ESP_OK;
}

static esp_netif_driver_ifconfig_t driver_cfg = {
    .handle = (void *)1,    // singleton driver, just to != NULL
    .transmit = transmit,
};

const esp_netif_driver_ifconfig_t *ppp_driver_cfg = &driver_cfg;

#define BUF_SIZE (1024)
#define CONNECTED "CONNECT 115200"

void ppp_task(void *args)
{
    struct ppp_info_t *ppp_info = args;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    QueueHandle_t event_queue;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE, 0, 16, &event_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, CONFIG_EXAMPLE_PPP_UART_TX_PIN, CONFIG_EXAMPLE_PPP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_rx_timeout(UART_NUM_1, 1));

    char *buffer = malloc(BUF_SIZE);
    ppp_info->context = buffer;
    const struct seq_t {
        const char *cmd;
        const char *expect;
        bool allow_fail;
    } init_sequence[] = {
        { .cmd = "AT\r\n", .expect = "OK" },
        { .cmd = "AT+CGDCONT=1,\"IP\",\"" CONFIG_EXAMPLE_MODEM_PPP_APN "\"\r\n", .expect = "OK" },
        { .cmd = "ATD*99##\r\n", .expect = "CONNECT", .allow_fail = true },
        { .cmd = "ATO\r\n", .expect = "CONNECT" },
    };
    int cmd_i = 0;
    int retry = 0;
    char *reply = buffer;
    const int max_retries = 3;
    uart_event_t event;
    uart_write_bytes(UART_NUM_1, "+++", 3);
    vTaskDelay(pdMS_TO_TICKS(1000));
    while (retry < max_retries) {
        ESP_LOGD(TAG, "Sending command: %s", init_sequence[cmd_i].cmd);
        uart_write_bytes(UART_NUM_1, init_sequence[cmd_i].cmd, strlen(init_sequence[cmd_i].cmd));
        xQueueReceive(event_queue, &event, pdMS_TO_TICKS(pdMS_TO_TICKS(1000)));
        size_t len;
        uart_get_buffered_data_len(UART_NUM_1, &len);
        if (!len) {
            continue;
        }
        len = uart_read_bytes(UART_NUM_1, reply, BUF_SIZE, 0);
        ESP_LOGD(TAG, "Received: %.*s", len, reply);
        if (strstr(reply, init_sequence[cmd_i].expect) || init_sequence[cmd_i].allow_fail) {
            if (strstr(reply, CONNECTED)) { // are we connected already?
                break;
            }
            cmd_i++;
            continue;
        }
        ++retry;
        vTaskDelay(pdMS_TO_TICKS(retry * 1000));
    }
    if (retry >= max_retries) {
        ESP_LOGE(TAG, "Failed to perform initial modem connection");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Modem configured correctly, switching to PPP protocol");
    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, ppp_info->parent.netif);
    esp_netif_action_start(ppp_info->parent.netif, 0, 0, 0);
    while (!ppp_info->stop_task) {
        xQueueReceive(event_queue, &event, pdMS_TO_TICKS(pdMS_TO_TICKS(1000)));
        if (event.type == UART_DATA) {
            size_t len;
            uart_get_buffered_data_len(UART_NUM_1, &len);
            if (len) {
                len = uart_read_bytes(UART_NUM_1, buffer, BUF_SIZE, 0);
                ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", buffer, len, ESP_LOG_VERBOSE);
                esp_netif_receive(ppp_info->parent.netif, buffer, len, NULL);
            }
        } else {
            ESP_LOGW(TAG, "Received UART event: %d", event.type);
        }
    }
}

void ppp_destroy_context(struct ppp_info_t *ppp_info)
{
    char *buffer = ppp_info->context;
    ppp_info->stop_task = true;
    vTaskDelay(pdMS_TO_TICKS(1000));
    free(buffer);
    uart_driver_delete(UART_NUM_1);
}
