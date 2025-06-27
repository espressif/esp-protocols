/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_at.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_check.h"

extern uint8_t g_at_cmd_port;

static uint8_t at_test_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "test command: <AT%s=?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "query command: <AT%s?> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cmd_test(uint8_t para_num)
{
    uint8_t index = 0;
    printf("setup command: <AT%s=%d> is executed\r\n", esp_at_get_current_cmd_name(), para_num);

    // get first parameter, and parse it into a digit
    int32_t digit = 0;
    if (esp_at_get_para_as_digit(index++, &digit) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    printf("digit: %d\r\n", digit);

    // get second parameter, and parse it into a string
    uint8_t *str = NULL;
    if (esp_at_get_para_as_str(index++, &str) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    printf("string: %s\r\n", str);

    // allocate a buffer and construct the data, then send the data to mcu via interface (uart/spi/sdio/socket)
    uint8_t *buffer = (uint8_t *)malloc(512);
    if (!buffer) {
        return ESP_AT_RESULT_CODE_ERROR;
    }
    int len = snprintf((char *)buffer, 512, "setup command: <AT%s=%d,\"%s\"> is executed\r\n",
                       esp_at_get_current_cmd_name(), digit, str);
    esp_at_port_write_data(buffer, len);

    // remember to free the buffer
    free(buffer);

    return ESP_AT_RESULT_CODE_OK;
}

#define TAG "at_custom_cmd"
static esp_netif_t *s_netif = NULL;

static void on_ppp_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_netif_t **netif = data;
    if (base == NETIF_PPP_STATUS && event_id == NETIF_PPP_ERRORUSER) {
        printf("Disconnected!");
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    esp_netif_t *netif = event->esp_netif;
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        printf("Got IPv4 event: Interface \"%s(%s)\" address: " IPSTR, esp_netif_get_desc(netif),
               esp_netif_get_ifkey(netif), IP2STR(&event->ip_info.ip));
        ESP_ERROR_CHECK(esp_netif_napt_enable(s_netif));

    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Disconnected");
    }
}

static SemaphoreHandle_t at_sync_sema = NULL;
static void wait_data_callback(void)
{
    static uint8_t buffer[1024] = {0};
    // xSemaphoreGive(at_sync_sema);
    int len = esp_at_port_read_data(buffer, sizeof(buffer) - 1);
    ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", buffer, len, ESP_LOG_VERBOSE);
    esp_netif_receive(s_netif, buffer, len, NULL);
}

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    // struct eppp_handle *handle = h;
    printf("transmit: %d bytes\n", len);
    // ESP_LOG_BUFFER_HEXDUMP("ppp_uart_send", buffer, len, ESP_LOG_INFO);
    esp_at_port_write_data(buffer, len);
    return ESP_OK;
}

static uint8_t at_exe_cmd_test(uint8_t *cmd_name)
{
    uint8_t buffer[64] = {0};
    snprintf((char *)buffer, 64, "execute command: <AT%s> is executed\r\n", cmd_name);
    esp_at_port_write_data(buffer, strlen((char *)buffer));
    printf("YYYEEES Command <AT%s> executed successfully\r\n", cmd_name);
    if (!at_sync_sema) {
        at_sync_sema = xSemaphoreCreateBinary();
        assert(at_sync_sema != NULL);
        esp_netif_driver_ifconfig_t driver_cfg = {
            .handle = (void *)1,
            .transmit = transmit,
        };
        const esp_netif_driver_ifconfig_t *ppp_driver_cfg = &driver_cfg;

        esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
        esp_netif_config_t netif_ppp_config = { .base = &base_netif_cfg,
                                                .driver = ppp_driver_cfg,
                                                .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP
                                              };

        s_netif = esp_netif_new(&netif_ppp_config);
        esp_netif_ppp_config_t netif_params;
        ESP_ERROR_CHECK(esp_netif_ppp_get_params(s_netif, &netif_params));
        netif_params.ppp_our_ip4_addr.addr = ESP_IP4TOADDR(192, 168, 11, 1);
        netif_params.ppp_their_ip4_addr.addr = ESP_IP4TOADDR(192, 168, 11, 2);
        netif_params.ppp_error_event_enabled = true;
        ESP_ERROR_CHECK(esp_netif_ppp_set_params(s_netif, &netif_params));
        if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL) != ESP_OK) {
            printf("Failed to register IP event handler");
        }
        if (esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_ppp_event, NULL) !=  ESP_OK) {
            printf("Failed to register NETIF_PPP_STATUS event handler");
        }


    }
    esp_at_port_write_data((uint8_t *)"CONNECT\r\n", strlen("CONNECT\r\n"));

    // set the callback function which will be called by AT port after receiving the input data
    esp_at_port_enter_specific(wait_data_callback);
    esp_netif_action_start(s_netif, 0, 0, 0);
    esp_netif_action_connected(s_netif, 0, 0, 0);

    // receive input data
    // while(xSemaphoreTake(at_sync_sema, portMAX_DELAY)) {
    //     int len = esp_at_port_read_data(buffer, sizeof(buffer) - 1);
    //     if (len > 0) {
    //         buffer[len] = '\0'; // null-terminate the string
    //         printf("Received data: %s\n", buffer);
    //     } else {
    //         printf("No data received or error occurred.\n");
    //         continue;
    //     }
    // }

    // uart_write_bytes(g_at_cmd_port, "CONNECT\r\n", strlen("CONNECT\r\n"));
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("-");
    }
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_test_cereg(uint8_t *cmd_name)
{
    printf("%s: AT command <AT%s> is executed\r\n", __func__, cmd_name);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_query_cereg(uint8_t *cmd_name)
{
    printf("%s: AT command <AT%s> is executed\r\n", __func__, cmd_name);
    // static uint8_t buffer[] = "+CEREG: 0,1,2,3,4,5\r\n";
    static uint8_t buffer[] = "+CEREG: 7,8\r\n";
    esp_at_port_write_data(buffer, sizeof(buffer));
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setup_cereg(uint8_t num)
{
    printf("%s: AT command <AT%d> is executed\r\n", __func__, num);
    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_exe_cereg(uint8_t *cmd_name)
{
    printf("%s: AT command <AT%s> is executed\r\n", __func__, cmd_name);
    return ESP_AT_RESULT_CODE_OK;
}


static const esp_at_cmd_struct at_custom_cmd[] = {
    {"+PPPD", at_test_cmd_test, at_query_cmd_test, at_setup_cmd_test, at_exe_cmd_test},
    {"+CEREG", at_test_cereg, at_query_cereg, at_setup_cereg, at_exe_cereg},
    /**
     * @brief You can define your own AT commands here.
     */
};

bool esp_at_custom_cmd_register(void)
{
    return esp_at_custom_cmd_array_regist(at_custom_cmd, sizeof(at_custom_cmd) / sizeof(esp_at_cmd_struct));
}

ESP_AT_CMD_SET_INIT_FN(esp_at_custom_cmd_register, 1);
