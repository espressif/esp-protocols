/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "eppp_link.h"
#include "eppp_transport.h"
#include "eppp_transport_spi.h"
#define TAG "eppp_spi"


static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    struct eppp_handle *handle = h;
#if CONFIG_EPPP_LINK_DEVICE_SPI
    struct packet buf = { };
    uint8_t *current_buffer = buffer;
    size_t remaining = len;
    do {    // TODO(IDF-9194): Refactor this loop to allocate only once and perform
        //       fragmentation after receiving from the queue (applicable only if MTU > MAX_PAYLOAD)
        size_t batch = remaining > MAX_PAYLOAD ? MAX_PAYLOAD : remaining;
        buf.data = malloc(batch);
        if (buf.data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate packet");
            return ESP_ERR_NO_MEM;
        }
        buf.len = batch;
        remaining -= batch;
        memcpy(buf.data, current_buffer, batch);
        current_buffer += batch;
        BaseType_t ret = xQueueSend(handle->out_queue, &buf, 0);
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "Failed to queue packet to slave!");
            return ESP_ERR_NO_MEM;
        }
    } while (remaining > 0);

    if (!handle->is_master && handle->blocked == SLAVE_BLOCKED) {
        uint32_t now = esp_timer_get_time();
        uint32_t diff = now - handle->slave_last_edge;
        if (diff < MIN_TRIGGER_US) {
            esp_rom_delay_us(MIN_TRIGGER_US - diff);
        }
        gpio_set_level(handle->gpio_intr, 0);
    }

#elif CONFIG_EPPP_LINK_DEVICE_UART
    ESP_LOG_BUFFER_HEXDUMP("ppp_uart_send", buffer, len, ESP_LOG_WARN);
    uart_write_bytes(handle->uart_port, buffer, len);
#endif // DEVICE UART or SPI
    return ESP_OK;
}

esp_err_t init_master(struct eppp_config_spi_s *config, struct eppp_handle *h);
esp_err_t init_slave(struct eppp_config_spi_s *config, struct eppp_handle *h);

static esp_err_t init_driver(eppp_transport_handle_t h, struct eppp_config_spi_s *config)
{
    if (config->is_master) {
        h->is_master = true;
        return init_master(config, h);
    }
    h->is_master = false;
    return init_slave(config, h);
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


eppp_transport_handle_t eppp_spi_init(struct eppp_config_spi_s *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    eppp_transport_handle_t h = calloc(1, sizeof(struct eppp_handle));
    ESP_RETURN_ON_FALSE(h, NULL, TAG, "Failed to allocate eppp_handle");
    h->base.post_attach = post_attach;
    ESP_GOTO_ON_ERROR(init_driver(h, config) != ESP_OK, err, TAG, "Failed to init SPI driver");
    return h;
err:
    free(h);
    return NULL;
}
void eppp_spi_deinit(eppp_transport_handle_t h)
{

//    esp_eth_stop(s_eth_handles[0]);
//    eppp_transport_ethernet_deinit(s_eth_handles);
}
