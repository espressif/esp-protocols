/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif_ppp.h"
#include "eppp_link.h"
#include "eppp_transport.h"
#include "driver/uart.h"

#define TAG "eppp_uart"

struct eppp_uart {
    struct eppp_handle parent;
    QueueHandle_t uart_event_queue;
    uart_port_t uart_port;
};

#define BUF_SIZE (1024)

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    struct eppp_handle *common = h;
    struct eppp_uart *handle = __containerof(common, struct eppp_uart, parent);;
    ESP_LOG_BUFFER_HEXDUMP("ppp_uart_send", buffer, len, ESP_LOG_WARN);
    uart_write_bytes(handle->uart_port, buffer, len);
    return ESP_OK;
}

static esp_err_t init_uart(struct eppp_uart *h, struct eppp_config_uart_s *config)
{
    h->uart_port = config->port;
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->baud;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_driver_install(h->uart_port, config->rx_buffer_size, 0, config->queue_size, &h->uart_event_queue, 0), TAG, "Failed to install UART");
    ESP_RETURN_ON_ERROR(uart_param_config(h->uart_port, &uart_config), TAG, "Failed to set params");
    ESP_RETURN_ON_ERROR(uart_set_pin(h->uart_port, config->tx_io, config->rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "Failed to set UART pins");
    ESP_RETURN_ON_ERROR(uart_set_rx_timeout(h->uart_port, 1), TAG, "Failed to set UART Rx timeout");
    return ESP_OK;
}

static void deinit_uart(struct eppp_uart *h)
{
    uart_driver_delete(h->uart_port);
}

esp_err_t eppp_perform(esp_netif_t *netif)
{
    static uint8_t buffer[BUF_SIZE] = {};
    struct eppp_handle *handle = esp_netif_get_io_driver(netif);
    struct eppp_uart *h = __containerof(handle, struct eppp_uart, parent);

    uart_event_t event = {};
    if (h->parent.stop) {
        return ESP_ERR_TIMEOUT;
    }

    if (xQueueReceive(h->uart_event_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_OK;
    }
    if (event.type == UART_DATA) {
        size_t len;
        uart_get_buffered_data_len(h->uart_port, &len);
        if (len) {
            len = uart_read_bytes(h->uart_port, buffer, BUF_SIZE, 0);
            ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", buffer, len, ESP_LOG_INFO);
            esp_netif_receive(netif, buffer, len, NULL);
        }
    } else {
        ESP_LOGW(TAG, "Received UART event: %d", event.type);
    }
    return ESP_OK;
}


static esp_err_t post_attach(esp_netif_t *esp_netif, void *args)
{
    eppp_transport_handle_t h = (eppp_transport_handle_t)args;
    h->base.netif = esp_netif;

    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  h,
        .transmit = transmit,
    };

    ESP_RETURN_ON_ERROR(esp_netif_set_driver_config(esp_netif, &driver_ifconfig), TAG, "Failed to set driver config");
    ESP_LOGI(TAG, "EPPP SPI transport attached to EPPP netif %s", esp_netif_get_desc(esp_netif));
//    ESP_RETURN_ON_ERROR(start_driver(h), TAG, "Failed to start EPPP SPI driver");
//    ESP_LOGI(TAG, "EPPP Ethernet driver started");
    return ESP_OK;

}

eppp_transport_handle_t eppp_uart_init(struct eppp_config_uart_s *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    struct eppp_uart *h = calloc(1, sizeof(struct eppp_uart));
    ESP_RETURN_ON_FALSE(h, NULL, TAG, "Failed to allocate eppp_handle");
    h->parent.base.post_attach = post_attach;
    ESP_GOTO_ON_ERROR(init_uart(h, config), err, TAG, "Failed to init UART");
    return &h->parent;
err:
    return NULL;
}

void eppp_uart_deinit(eppp_transport_handle_t handle)
{
    struct eppp_uart *h = __containerof(handle, struct eppp_uart, parent);
    deinit_uart(h);
    free(h);
}
