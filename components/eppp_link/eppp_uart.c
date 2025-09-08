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
#include "eppp_link.h"
#include "eppp_transport.h"
#include "driver/uart.h"

#define TAG "eppp_uart"

struct eppp_uart {
    struct eppp_handle parent;
    QueueHandle_t uart_event_queue;
    uart_port_t uart_port;
};

#define MAX_PAYLOAD (1500)
#define HEADER_MAGIC (0x7E)
#define HEADER_SIZE (4)
#define MAX_PACKET_SIZE (MAX_PAYLOAD + HEADER_SIZE)
/* Maximum size of a packet sent over UART, including header and payload */
#define UART_BUF_SIZE   (MAX_PACKET_SIZE)

struct header {
    uint8_t magic;
    uint8_t channel;
    uint8_t check;
    uint16_t size;
} __attribute__((packed));

static esp_err_t transmit_generic(struct eppp_uart *handle, int channel, void *buffer, size_t len)
{
#ifndef CONFIG_EPPP_LINK_USES_PPP
    static uint8_t out_buf[MAX_PACKET_SIZE] = {};
    struct header *head = (void *)out_buf;
    head->magic = HEADER_MAGIC;
    head->check = 0;
    head->channel = channel;
    head->size = len;
    head->check = (0xFF & len) ^ (len >> 8);
    memcpy(out_buf + sizeof(struct header), buffer, len);
    ESP_LOG_BUFFER_HEXDUMP("ppp_uart_send", out_buf, len + sizeof(struct header), ESP_LOG_DEBUG);
    uart_write_bytes(handle->uart_port, out_buf, len + sizeof(struct header));
#else
    ESP_LOG_BUFFER_HEXDUMP("ppp_uart_send", buffer, len, ESP_LOG_DEBUG);
    uart_write_bytes(handle->uart_port, buffer, len);
#endif
    return ESP_OK;
}

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    struct eppp_handle *handle = h;
    struct eppp_uart *uart_handle = __containerof(handle, struct eppp_uart, parent);
    return transmit_generic(uart_handle, 0, buffer, len);
}

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
static esp_err_t transmit_channel(esp_netif_t *netif, int channel, void *buffer, size_t len)
{
    struct eppp_handle *handle = esp_netif_get_io_driver(netif);
    struct eppp_uart *uart_handle = __containerof(handle, struct eppp_uart, parent);
    return transmit_generic(uart_handle, channel, buffer, len);
}
#endif

static esp_err_t init_uart(struct eppp_uart *h, struct eppp_config_uart_s *config)
{
    h->uart_port = config->port;
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->baud;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = config->flow_control;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_driver_install(h->uart_port, config->rx_buffer_size, 0, config->queue_size, &h->uart_event_queue, 0), TAG, "Failed to install UART");
    ESP_RETURN_ON_ERROR(uart_param_config(h->uart_port, &uart_config), TAG, "Failed to set params");
    ESP_RETURN_ON_ERROR(uart_set_pin(h->uart_port, config->tx_io, config->rx_io, config->rts_io, config->cts_io), TAG, "Failed to set UART pins");
    ESP_RETURN_ON_ERROR(uart_set_rx_timeout(h->uart_port, 1), TAG, "Failed to set UART Rx timeout");
    return ESP_OK;
}

static void deinit_uart(struct eppp_uart *h)
{
    uart_driver_delete(h->uart_port);
}

#ifndef CONFIG_EPPP_LINK_USES_PPP
/**
 * @brief Process incoming UART data and extract packets
 */
static void process_packet(esp_netif_t *netif, uart_port_t uart_port, size_t available_data)
{
    static uint8_t in_buf[2 * UART_BUF_SIZE] = {};
    static size_t buf_start = 0;
    static size_t buf_end = 0;
    struct header *head;

    // Read data directly into our buffer
    size_t available_space = sizeof(in_buf) - buf_end;
    size_t read_size = (available_data < available_space) ? available_data : available_space;
    if (read_size > 0) {
        size_t len = uart_read_bytes(uart_port, in_buf + buf_end, read_size, 0);
        ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", in_buf + buf_end, len, ESP_LOG_DEBUG);

        if (buf_end + len <= sizeof(in_buf)) {
            buf_end += len;
        } else {
            ESP_LOGW(TAG, "Buffer overflow, discarding data");
            buf_start = buf_end = 0;
            return;
        }
    }

    // Process while we have enough data for at least a header
    while ((buf_end - buf_start) >= sizeof(struct header)) {
        head = (void *)(in_buf + buf_start);

        if (head->magic != HEADER_MAGIC) {
            goto recover;
        }

        uint8_t calculated_check = (head->size & 0xFF) ^ (head->size >> 8);
        if (head->check != calculated_check) {
            ESP_LOGW(TAG, "Checksum mismatch: expected 0x%04x, got 0x%04x", calculated_check, head->check);
            goto recover;
        }

        // Check if we have the complete packet
        uint16_t payload_size = head->size;
        int channel = head->channel;
        size_t total_packet_size = sizeof(struct header) + payload_size;

        if (payload_size > MAX_PAYLOAD) {
            ESP_LOGW(TAG, "Invalid payload size: %d", payload_size);
            goto recover;
        }

        // If we don't have the complete packet yet, wait for more data
        if ((buf_end - buf_start) < total_packet_size) {
            ESP_LOGD(TAG, "Incomplete packet: got %d bytes, need %d bytes", (buf_end - buf_start), total_packet_size);
            break;
        }

        // Got a complete packet, pass it to network
        if (channel == 0) {
            esp_netif_receive(netif, in_buf + buf_start + sizeof(struct header), payload_size, NULL);
        } else {
#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
            struct eppp_handle *handle = esp_netif_get_io_driver(netif);
            struct eppp_uart *h = __containerof(handle, struct eppp_uart, parent);
            if (h->parent.channel_rx) {
                h->parent.channel_rx(netif, channel, in_buf + buf_start + sizeof(struct header), payload_size);
            }
#endif
        }

        // Advance start pointer past this packet
        buf_start += total_packet_size;

        // compact if we don't have enough space for 1x UART_BUF_SIZE
        if (buf_start > (sizeof(in_buf) / 2) || (sizeof(in_buf) - buf_end) < UART_BUF_SIZE) {
            if (buf_start < buf_end) {
                size_t remaining_data = buf_end - buf_start;
                memmove(in_buf, in_buf + buf_start, remaining_data);
                buf_end = remaining_data;
            } else {
                buf_end = 0;
            }
            buf_start = 0;
        }

        continue;

recover:
        // Search for next HEADER_MAGIC occurrence
        uint8_t *next_magic = memchr(in_buf + buf_start + 1, HEADER_MAGIC, buf_end - buf_start - 1);
        if (next_magic) {
            // Found next potential header, advance start to that position
            buf_start = next_magic - in_buf;

            // Check if we need to compact after recovery too
            if (buf_start > (sizeof(in_buf) / 2) || (sizeof(in_buf) - buf_end) < UART_BUF_SIZE) {
                if (buf_start < buf_end) {
                    size_t remaining_data = buf_end - buf_start;
                    memmove(in_buf, in_buf + buf_start, remaining_data);
                    buf_end = remaining_data;
                } else {
                    buf_end = 0;
                }
                buf_start = 0;
            }
        } else {
            // No more HEADER_MAGIC found, discard all data
            buf_start = buf_end = 0;
        }
    }
}
#endif

esp_err_t eppp_perform(esp_netif_t *netif)
{
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
#ifdef CONFIG_EPPP_LINK_USES_PPP
            static uint8_t buffer[UART_BUF_SIZE] = {};
            len = uart_read_bytes(h->uart_port, buffer, UART_BUF_SIZE, 0);
            ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", buffer, len, ESP_LOG_DEBUG);
            esp_netif_receive(netif, buffer, len, NULL);
#else
            // Read directly in process_packet to save one buffer
            process_packet(netif, h->uart_port, len);
#endif
        }
    } else {
        ESP_LOGW(TAG, "Received UART event: %d", event.type);
    }
    return ESP_OK;
}


static esp_err_t post_attach(esp_netif_t *esp_netif, void *args)
{
    eppp_transport_handle_t h = (eppp_transport_handle_t)args;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "Transport handle cannot be null");
    h->base.netif = esp_netif;

    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  h,
        .transmit = transmit,
    };

    ESP_RETURN_ON_ERROR(esp_netif_set_driver_config(esp_netif, &driver_ifconfig), TAG, "Failed to set driver config");
    ESP_LOGI(TAG, "EPPP UART transport attached to EPPP netif %s", esp_netif_get_desc(esp_netif));
    return ESP_OK;
}

eppp_transport_handle_t eppp_uart_init(struct eppp_config_uart_s *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config, NULL, TAG, "Config cannot be null");
    struct eppp_uart *h = calloc(1, sizeof(struct eppp_uart));
    ESP_RETURN_ON_FALSE(h, NULL, TAG, "Failed to allocate eppp_handle");
#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
    h->parent.channel_tx = transmit_channel;
#endif
    h->parent.base.post_attach = post_attach;
    ESP_GOTO_ON_ERROR(init_uart(h, config), err, TAG, "Failed to init UART");
    return &h->parent;
err:
    free(h);
    return NULL;
}

void eppp_uart_deinit(eppp_transport_handle_t handle)
{
    struct eppp_uart *h = __containerof(handle, struct eppp_uart, parent);
    deinit_uart(h);
    free(h);
}
