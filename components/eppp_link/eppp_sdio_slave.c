/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "driver/sdio_slave.h"
#include "eppp_sdio.h"


#if CONFIG_SOC_SDIO_SLAVE_SUPPORTED
#define BUFFER_NUM 2

static WORD_ALIGNED_ATTR uint8_t sdio_slave_rx_buffer[BUFFER_NUM][SDIO_PAYLOAD];
static WORD_ALIGNED_ATTR uint8_t sdio_slave_tx_buffer[SDIO_PAYLOAD];

static const char *TAG = "eppp_sdio_slave";
static int s_slave_request = 0;

esp_err_t eppp_sdio_slave_tx(void *h, void *buffer, size_t len)
{
    if (s_slave_request != REQ_INIT) {
        // silently skip the Tx if the SDIO not fully initialized
        return ESP_OK;
    }
    memcpy(sdio_slave_tx_buffer, buffer, len);
    size_t send_len = SDIO_ALIGN(len);
    if (send_len > len) {
        // pad with SOF's if the size is not 4 bytes aligned
        memset(&sdio_slave_tx_buffer[len], 0x7E, send_len - len);
    }

    ESP_LOG_BUFFER_HEXDUMP(TAG, sdio_slave_tx_buffer, send_len, ESP_LOG_VERBOSE);
    esp_err_t ret = sdio_slave_transmit(sdio_slave_tx_buffer, send_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sdio slave transmit error, ret : 0x%x", ret);
        // to inform the upper layers
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t slave_reset(void)
{
    ESP_LOGI(TAG, "SDIO slave reset");
    esp_err_t ret;
    sdio_slave_stop();
    ret = sdio_slave_reset();
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sdio_slave_start();
    if (ret != ESP_OK) {
        return ret;
    }

    while (1) {
        sdio_slave_buf_handle_t handle;
        ret = sdio_slave_send_get_finished(&handle, 0);
        if (ret != ESP_OK) {
            break;
        }
        ret = sdio_slave_recv_load_buf(handle);
        if (ret != ESP_OK) {
            break;
        }
    }
    return ESP_OK;
}

esp_err_t eppp_sdio_slave_rx(esp_netif_t *netif)
{
    if (s_slave_request == REQ_RESET) {
        ESP_LOGW(TAG, "job: %x", s_slave_request);
        slave_reset();
        s_slave_request = REQ_INIT;
    }
    const TickType_t blocking = portMAX_DELAY;
    sdio_slave_buf_handle_t recv_queue[BUFFER_NUM];
    int packet_size = 0;

    sdio_slave_buf_handle_t handle;
    esp_err_t ret = sdio_slave_recv_packet(&handle, pdMS_TO_TICKS(1000));
    if (ret == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    recv_queue[packet_size++] = handle;
    while (ret == ESP_ERR_NOT_FINISHED) {
        //The return value must be ESP_OK or ESP_ERR_NOT_FINISHED.
        ret = sdio_slave_recv_packet(&handle, blocking);
        recv_queue[packet_size++] = handle;
        ESP_LOGE(TAG, "again...");
    }
    int packet_len = 0;
    for (int i = 0; i < packet_size; i++) {
        size_t buf_len;
        sdio_slave_recv_get_buf(recv_queue[i], &buf_len);
        packet_len += buf_len;
    }
//    ESP_LOGD(TAG, "Packet received, len: %d", packet_len);
    for (int i = 0; i < packet_size; i++) {
        handle = recv_queue[i];
        size_t length;
        uint8_t *ptr = sdio_slave_recv_get_buf(handle, &length);

        printf("(%d)", i);
        ESP_LOGD(TAG, "Buffer %d, len: %d", i, length);
        ESP_LOG_BUFFER_HEXDUMP(TAG, ptr, length, ESP_LOG_VERBOSE);
//        ESP_LOG_BUFFER_HEXDUMP(TAG, ptr, length, ESP_LOG_ERROR);
        esp_netif_receive(netif, ptr, length, NULL);
        ret = sdio_slave_recv_load_buf(handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to recycle packet buffer");
            return ret;
        }
    }
    return ESP_OK;
}


static void event_cb(uint8_t pos)
{
    ESP_EARLY_LOGI(TAG, "SDIO event: %d", pos);
    if (pos == SLAVE_INTR) {
        s_slave_request = sdio_slave_read_reg(SLAVE_REG_REQ);
        sdio_slave_write_reg(SLAVE_REG_REQ, 0);
    }
//    switch (pos) {
//    case SLAVE_REG_REQ:
//
//        ESP_EARLY_LOGE(TAG, "job: %x", s_slave_request);
//        sdio_slave_write_reg(SLAVE_REG_REQ, REQ_INIT);
//        if (s_slave_request & BIT(REQ_INIT)) {
//            ESP_EARLY_LOGE(TAG, "job: %d");
//        }
//        break;
//    }
}

esp_err_t eppp_sdio_slave_init(void)
{
    sdio_slave_config_t config = {
        .sending_mode       = SDIO_SLAVE_SEND_PACKET,
        .send_queue_size    = BUFFER_NUM,
        .recv_buffer_size   = SDIO_PAYLOAD,
        .event_cb           = event_cb,
        .flags              = SDIO_SLAVE_FLAG_HIGH_SPEED,
        .timing             = SDIO_SLAVE_TIMING_NSEND_PSAMPLE,
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

    sdio_slave_set_host_intena(SDIO_SLAVE_HOSTINT_SEND_NEW_PACKET);

    ret = sdio_slave_start();
    if (ret != ESP_OK) {
        sdio_slave_deinit();
        return ESP_FAIL;
    }
    return ESP_OK;
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

esp_err_t eppp_sdio_slave_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

#endif // CONFIG_SOC_SDIO_SLAVE_SUPPORTED
