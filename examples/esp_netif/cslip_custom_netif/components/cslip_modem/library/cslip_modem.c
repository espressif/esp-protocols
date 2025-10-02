/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "cslip_modem.h"

#include "esp_netif.h"
#include "cslip_modem_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#define CSLIP_RX_TASK_PRIORITY   10
#define CSLIP_RX_TASK_STACK_SIZE (4 * 1024)

static const char *TAG = "cslip-modem";

typedef struct {
    uart_port_t uart_dev;
    uint32_t uart_baud;
    int uart_tx_pin;
    int uart_rx_pin;
    QueueHandle_t uart_queue;
    TaskHandle_t uart_rx_task;
} esp_cslip_uart_t;

struct cslip_modem {
    esp_netif_driver_base_t base;
    esp_cslip_uart_t uart;
    uint8_t *buffer;
    uint32_t buffer_len;
    cslip_rx_filter_cb_t *rx_filter;
    bool running;
    esp_ip6_addr_t addr;
    esp_cslip_config_t cslip_cfg; // currently informational
};

static void cslip_modem_uart_rx_task(void *arg);
static esp_err_t cslip_modem_post_attach(esp_netif_t *esp_netif, void *args);

cslip_modem_handle cslip_modem_create(esp_netif_t *cslip_netif, const cslip_modem_config_t *modem_config)
{
    if (cslip_netif == NULL || modem_config == NULL) {
        ESP_LOGE(TAG, "invalid parameters");
        return NULL;
    }
    ESP_LOGI(TAG, "%s: Creating cslip modem (netif: %p)", __func__, cslip_netif);

    cslip_modem_handle modem = calloc(1, sizeof(struct cslip_modem));
    if (!modem) {
        ESP_LOGE(TAG, "create netif glue failed");
        return NULL;
    }

    modem->base.post_attach = cslip_modem_post_attach;
    modem->base.netif = cslip_netif;

    modem->buffer_len = modem_config->rx_buffer_len;
    modem->rx_filter = modem_config->rx_filter;

    modem->uart.uart_dev = modem_config->uart_dev;
    modem->uart.uart_baud = modem_config->uart_baud;
    modem->uart.uart_rx_pin = modem_config->uart_rx_pin;
    modem->uart.uart_tx_pin = modem_config->uart_tx_pin;
    modem->addr = *modem_config->ipv6_addr;
    modem->cslip_cfg = modem_config->cslip;

    return modem;
}

static esp_err_t esp_cslip_driver_start(cslip_modem_handle modem)
{
    if (modem->buffer == NULL) {
        modem->buffer = malloc(modem->buffer_len);
    }
    if (modem->buffer == NULL) {
        ESP_LOGE(TAG, "error allocating rx buffer");
        return ESP_ERR_NO_MEM;
    }

    uart_config_t uart_config = {
        .baud_rate = modem->uart.uart_baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(modem->uart.uart_dev, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(modem->uart.uart_dev, modem->uart.uart_tx_pin, modem->uart.uart_rx_pin, 0, 0));
    ESP_ERROR_CHECK(uart_driver_install(modem->uart.uart_dev, modem->buffer_len, modem->buffer_len, 10, &modem->uart.uart_queue, 0));

    modem->running = true;
    xTaskCreate(cslip_modem_uart_rx_task, "cslip_modem_uart_rx_task", CSLIP_RX_TASK_STACK_SIZE, modem, CSLIP_RX_TASK_PRIORITY, &modem->uart.uart_rx_task);

    esp_netif_action_start(modem->base.netif, 0, 0, 0);
    ESP_ERROR_CHECK(cslip_modem_netif_start(modem->base.netif, &modem->addr));
    return ESP_OK;
}

esp_err_t cslip_modem_destroy(cslip_modem_handle modem)
{
    if (modem != NULL) {
        esp_netif_action_stop(modem->base.netif, 0, 0, 0);
        ESP_ERROR_CHECK(cslip_modem_netif_stop(modem->base.netif));
        vTaskDelete(modem->uart.uart_rx_task);
        uart_driver_delete(modem->uart.uart_dev);
        free(modem);
    }

    return ESP_OK;
}

static esp_err_t cslip_modem_transmit(void *driver, void *buffer, size_t len)
{
    cslip_modem_handle modem = (cslip_modem_handle)driver;
    int32_t res = uart_write_bytes(modem->uart.uart_dev, (char *)buffer, len);
    if (res < 0) {
        ESP_LOGE(TAG, "%s: uart_write_bytes error %" PRId32, __func__, res);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t cslip_modem_post_attach(esp_netif_t *esp_netif, void *args)
{
    cslip_modem_handle modem = (cslip_modem_handle) args;

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = NULL,
        .transmit = cslip_modem_transmit,
        .handle = modem,
    };

    modem->base.netif = esp_netif;
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    esp_cslip_driver_start(modem);
    return ESP_OK;
}

static void cslip_modem_uart_rx_task(void *arg)
{
    if (arg == NULL) {
        vTaskDelete(NULL);
    }
    cslip_modem_handle modem = (cslip_modem_handle) arg;

    ESP_LOGD(TAG, "Start CSLIP modem RX task (filter: %p)", modem->rx_filter);

    while (modem->running == true) {
        int len = uart_read_bytes(modem->uart.uart_dev, modem->buffer, modem->buffer_len, 1 / portTICK_PERIOD_MS);

        if (len > 0) {
            modem->buffer[len] = '\0';
            if ((modem->rx_filter != NULL) && modem->rx_filter(modem, modem->buffer, len)) {
                continue;
            }
            esp_netif_receive(modem->base.netif, modem->buffer, len, NULL);
        }
        vTaskDelay(1 * portTICK_PERIOD_MS);
    }
}

esp_ip6_addr_t cslip_modem_get_ipv6_address(cslip_modem_handle modem)
{
    return modem->addr;
}

void cslip_modem_raw_write(cslip_modem_handle modem, void *buffer, size_t len)
{
    cslip_modem_netif_raw_write(modem->base.netif, buffer, len);
}
