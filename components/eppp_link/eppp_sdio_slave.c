/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "driver/sdio_slave.h"
#include "eppp_link.h"
#include "eppp_transport.h"
#include "eppp_sdio.h"
#include "esp_check.h"
#if CONFIG_EPPP_LINK_DEVICE_SDIO_SLAVE
#define BUFFER_NUM 4
#define BUFFER_SIZE SDIO_PAYLOAD
static const char *TAG = "eppp_sdio_slave";
static DMA_ATTR uint8_t sdio_slave_rx_buffer[BUFFER_NUM][BUFFER_SIZE];
static DMA_ATTR uint8_t sdio_slave_tx_buffer[SDIO_PAYLOAD];
static int s_slave_request = 0;

static esp_err_t eppp_sdio_host_tx_generic(int channel, void *buffer, size_t len)
{
    if (s_slave_request != REQ_INIT) {
        // silently skip the Tx if the SDIO not fully initialized
        return ESP_OK;
    }
    struct header *head = (void *)sdio_slave_tx_buffer;
    head->magic = PPP_SOF;
    head->channel = channel;
    head->size = len;
    memcpy(sdio_slave_tx_buffer + sizeof(struct header), buffer, len);
    size_t send_len = SDIO_ALIGN(len + sizeof(struct header));
    ESP_LOG_BUFFER_HEXDUMP(TAG, sdio_slave_tx_buffer, send_len, ESP_LOG_VERBOSE);
    esp_err_t ret = sdio_slave_transmit(sdio_slave_tx_buffer, send_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sdio slave transmit error, ret : 0x%x", ret);
        // to inform the upper layers
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t eppp_sdio_slave_tx(void *h, void *buffer, size_t len)
{
    return eppp_sdio_host_tx_generic(0, buffer, len);
}

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
esp_err_t eppp_sdio_transmit_channel(esp_netif_t *netif, int channel, void *buffer, size_t len)
{
    return eppp_sdio_host_tx_generic(channel, buffer, len);
}
#endif

static esp_err_t slave_reset(void)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "SDIO slave reset");
    sdio_slave_stop();
    ESP_GOTO_ON_ERROR(sdio_slave_reset(), err, TAG, "slave reset failed");
    ESP_GOTO_ON_ERROR(sdio_slave_start(), err, TAG, "slave start failed");

    while (1) {
        sdio_slave_buf_handle_t handle;
        ret = sdio_slave_send_get_finished(&handle, 0);
        if (ret == ESP_ERR_TIMEOUT) {
            break;
        }
        ESP_GOTO_ON_ERROR(ret, err, TAG, "slave-get-finished failed");
        ESP_GOTO_ON_ERROR(sdio_slave_recv_load_buf(handle), err, TAG, "slave-load-buf failed");
    }
err:
    return ret;
}

esp_err_t eppp_sdio_slave_rx(esp_netif_t *netif)
{
    if (s_slave_request == REQ_RESET) {
        ESP_LOGD(TAG, "request: %x", s_slave_request);
        slave_reset();
        s_slave_request = REQ_INIT;
    }
    sdio_slave_buf_handle_t handle;
    size_t length;
    uint8_t *ptr;
    esp_err_t ret = sdio_slave_recv_packet(&handle, pdMS_TO_TICKS(1000));
    if (ret == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    if (ret == ESP_ERR_NOT_FINISHED || ret == ESP_OK) {
again:
        ptr = sdio_slave_recv_get_buf(handle, &length);
        struct header *head = (void *)ptr;
        if (head->magic != PPP_SOF) {
            ESP_LOGE(TAG, "invalid magic %x", head->magic);
            return ESP_FAIL;
        }
        if (head->channel > NR_OF_CHANNELS) {
            ESP_LOGE(TAG, "invalid channel %x", head->channel);
            return ESP_FAIL;
        }
        if (head->size > SDIO_PAYLOAD || head->size > length) {
            ESP_LOGE(TAG, "invalid size %x", head->size);
            return ESP_FAIL;
        }
        if (head->channel == 0) {
            esp_netif_receive(netif, ptr + sizeof(struct header), head->size, NULL);
        } else {
#if defined(CONFIG_EPPP_LINK_CHANNELS_SUPPORT)
            struct eppp_handle *h = esp_netif_get_io_driver(netif);
            if (h->channel_rx) {
                h->channel_rx(netif, head->channel, ptr + sizeof(struct header), head->size);
            }
#endif
        }
        if (sdio_slave_recv_load_buf(handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to recycle packet buffer");
            return ESP_FAIL;
        }
        if (ret == ESP_ERR_NOT_FINISHED) {
            ret = sdio_slave_recv_packet(&handle, 0);
            if (ret == ESP_ERR_NOT_FINISHED || ret == ESP_OK) {
                goto again;
            }
        }
        ESP_LOG_BUFFER_HEXDUMP(TAG, ptr, length, ESP_LOG_VERBOSE);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Error when receiving packet %d", ret);
    return ESP_FAIL;
}


static void event_cb(uint8_t pos)
{
    ESP_EARLY_LOGI(TAG, "SDIO event: %d", pos);
    if (pos == SLAVE_INTR) {
        s_slave_request = sdio_slave_read_reg(SLAVE_REG_REQ);
        sdio_slave_write_reg(SLAVE_REG_REQ, 0);
    }
}

esp_err_t eppp_sdio_slave_init(struct eppp_config_sdio_s *eppp_config)
{
    sdio_slave_config_t config = {
        .sending_mode       = SDIO_SLAVE_SEND_PACKET,
        .send_queue_size    = BUFFER_NUM,
        .recv_buffer_size   = BUFFER_SIZE,
        .event_cb           = event_cb,
    };
    esp_err_t ret = sdio_slave_initialize(&config);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < BUFFER_NUM; i++) {
        sdio_slave_buf_handle_t handle = sdio_slave_recv_register_buf(sdio_slave_rx_buffer[i]);
        if (handle == NULL) {
            sdio_slave_deinit();
            return ESP_FAIL;
        }
        ret = sdio_slave_recv_load_buf(handle);
        if (ret != ESP_OK) {
            sdio_slave_deinit();
            return ESP_FAIL;
        }
    }

    sdio_slave_set_host_intena(SDIO_SLAVE_HOSTINT_SEND_NEW_PACKET); // only need one interrupt to notify of a new packet

    ret = sdio_slave_start();
    if (ret != ESP_OK) {
        sdio_slave_deinit();
        return ESP_FAIL;
    }
    return ESP_OK;
}

void eppp_sdio_slave_deinit(void)
{
    sdio_slave_stop();
    sdio_slave_deinit();
}

#else // SOC_SDIO_SLAVE NOT-SUPPORTED

esp_err_t eppp_sdio_slave_tx(void *h, void *buffer, size_t len)
{
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t eppp_sdio_slave_rx(esp_netif_t *netif)
{
    return ESP_ERR_NOT_SUPPORTED;
}

void eppp_sdio_slave_deinit(void)
{
}

esp_err_t eppp_sdio_slave_init(struct eppp_config_sdio_s *config)
{
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // CONFIG_SOC_SDIO_SLAVE_SUPPORTED
