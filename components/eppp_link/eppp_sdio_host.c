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
#include "esp_serial_slave_link/essl_sdio.h"
#include "eppp_sdio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_check.h"
#include "eppp_link.h"
#include "eppp_transport.h"

#if CONFIG_EPPP_LINK_DEVICE_SDIO_HOST

// For blocking operations
#define TIMEOUT_MAX   UINT32_MAX
// Short timeout for sending/receiving ESSL packets
#define PACKET_TIMEOUT_MS   50

static const char *TAG = "eppp_sdio_host";
static SemaphoreHandle_t s_essl_mutex = NULL;
static essl_handle_t s_essl = NULL;
static sdmmc_card_t *s_card = NULL;

static DRAM_DMA_ALIGNED_ATTR uint8_t send_buffer[SDIO_PACKET_SIZE];
static DMA_ATTR uint8_t rcv_buffer[SDIO_PACKET_SIZE];

static esp_err_t eppp_sdio_host_tx_generic(int channel, void *buffer, size_t len)
{
    if (s_essl == NULL || s_essl_mutex == NULL) {
        // silently skip the Tx if the SDIO not fully initialized
        return ESP_OK;
    }


    struct header *head = (void *)send_buffer;
    head->magic = PPP_SOF;
    head->channel = channel;
    head->size = len;
    memcpy(send_buffer + sizeof(struct header), buffer, len);
    size_t send_len = SDIO_ALIGN(len + sizeof(struct header));
    xSemaphoreTake(s_essl_mutex, portMAX_DELAY);
    esp_err_t ret = essl_send_packet(s_essl, send_buffer, send_len, PACKET_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Slave not ready to receive packet %x", ret);
        vTaskDelay(pdMS_TO_TICKS(1000));
        ret = ESP_ERR_NO_MEM; // to inform the upper layers
    }
    ESP_LOG_BUFFER_HEXDUMP(TAG, send_buffer, send_len, ESP_LOG_VERBOSE);
    xSemaphoreGive(s_essl_mutex);
    return ret;
}

esp_err_t eppp_sdio_host_tx(void *h, void *buffer, size_t len)
{
    return eppp_sdio_host_tx_generic(0, buffer, len);
}

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
esp_err_t eppp_sdio_transmit_channel(esp_netif_t *netif, int channel, void *buffer, size_t len)
{
    return eppp_sdio_host_tx_generic(channel, buffer, len);
}
#endif


static esp_err_t request_slave_reset(void)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "send reset to slave...");
    ESP_GOTO_ON_ERROR(essl_write_reg(s_essl, SLAVE_REG_REQ, REQ_RESET, NULL, TIMEOUT_MAX), err, TAG, "write-reg failed");
    ESP_GOTO_ON_ERROR(essl_send_slave_intr(s_essl, BIT(SLAVE_INTR), TIMEOUT_MAX), err, TAG, "send-intr failed");
    vTaskDelay(pdMS_TO_TICKS(PACKET_TIMEOUT_MS));
    ESP_GOTO_ON_ERROR(essl_wait_for_ready(s_essl, TIMEOUT_MAX), err, TAG, "wait-for-ready failed");
    ESP_LOGI(TAG, "slave io ready");
err:
    return ret;
}

esp_err_t eppp_sdio_host_init(struct eppp_config_sdio_s *eppp_config)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(sdmmc_host_init(), err, TAG, "sdmmc host init failed");

    // configure SDIO interface and slot
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = eppp_config->width;
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = eppp_config->clk;
    slot_config.cmd = eppp_config->cmd;
    slot_config.d0  = eppp_config->d0;
    slot_config.d1  = eppp_config->d1;
    slot_config.d2  = eppp_config->d2;
    slot_config.d3  = eppp_config->d3;
#endif

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config), err, TAG, "init sdmmc host slot failed");

    sdmmc_host_t config = SDMMC_HOST_DEFAULT();
    config.flags = SDMMC_HOST_FLAG_4BIT;
    config.flags |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;
    config.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    s_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    ESP_GOTO_ON_FALSE(s_card, ESP_ERR_NO_MEM, err, TAG, "card allocation failed");
    ESP_GOTO_ON_ERROR(sdmmc_card_init(&config, s_card), err, TAG, "sdmmc card init failed");

    essl_sdio_config_t ser_config = {
        .card = s_card,
        .recv_buffer_size = SDIO_PAYLOAD,
    };
    ESP_GOTO_ON_FALSE(essl_sdio_init_dev(&s_essl, &ser_config) == ESP_OK && s_essl, ESP_FAIL, err, TAG, "essl_sdio_init_dev failed");
    ESP_GOTO_ON_ERROR(essl_init(s_essl, TIMEOUT_MAX), err, TAG, "essl-init failed");
    ESP_GOTO_ON_ERROR(request_slave_reset(), err, TAG, "failed to reset the slave");
    ESP_GOTO_ON_FALSE((s_essl_mutex = xSemaphoreCreateMutex()), ESP_ERR_NO_MEM, err, TAG, "failed to create semaphore");
    return ret;

err:
    essl_sdio_deinit_dev(s_essl);
    s_essl = NULL;
    return ret;
}

static esp_err_t get_intr(uint32_t *out_raw)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(essl_get_intr(s_essl, out_raw, NULL, 0), err, TAG, "essl-get-int failed");
    ESP_GOTO_ON_ERROR(essl_clear_intr(s_essl, *out_raw, 0), err, TAG, "essl-clear-int failed");
    ESP_LOGD(TAG, "intr: %08"PRIX32, *out_raw);
err:
    return ret;
}

esp_err_t eppp_sdio_host_rx(esp_netif_t *netif)
{
    uint32_t intr;
    esp_err_t err = essl_wait_int(s_essl, TIMEOUT_MAX);
    if (err == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    xSemaphoreTake(s_essl_mutex, portMAX_DELAY);
    err = get_intr(&intr);
    if (err == ESP_ERR_TIMEOUT) {
        xSemaphoreGive(s_essl_mutex);
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to check for interrupts %d", err);
        xSemaphoreGive(s_essl_mutex);
        return ESP_FAIL;
    }
    if (intr & ESSL_SDIO_DEF_ESP32.new_packet_intr_mask) {
        esp_err_t ret;
        do {
            size_t size_read = SDIO_PACKET_SIZE;
            ret = essl_get_packet(s_essl, rcv_buffer, SDIO_PACKET_SIZE, &size_read, PACKET_TIMEOUT_MS);
            if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "interrupt but no data can be read");
                break;
            } else if (ret == ESP_OK) {
                ESP_LOGD(TAG, "receive data, size: %d", size_read);
                struct header *head = (void *)rcv_buffer;
                if (head->magic != PPP_SOF) {
                    ESP_LOGE(TAG, "invalid magic %x", head->magic);
                    break;
                }
                if (head->channel > NR_OF_CHANNELS) {
                    ESP_LOGE(TAG, "invalid channel %x", head->channel);
                    break;
                }
                if (head->size > SDIO_PAYLOAD || head->size > size_read) {
                    ESP_LOGE(TAG, "invalid size %x", head->size);
                    break;
                }
                ESP_LOG_BUFFER_HEXDUMP(TAG, rcv_buffer, size_read, ESP_LOG_VERBOSE);
                if (head->channel == 0) {
                    esp_netif_receive(netif, rcv_buffer + sizeof(struct header), head->size, NULL);
                } else {
#if defined(CONFIG_EPPP_LINK_CHANNELS_SUPPORT)
                    struct eppp_handle *h = esp_netif_get_io_driver(netif);
                    if (h->channel_rx) {
                        h->channel_rx(netif, head->channel, rcv_buffer + sizeof(struct header), head->size);
                    }
#endif
                }
                break;
            } else {
                ESP_LOGE(TAG, "rx packet error: %08X", ret);
                if (request_slave_reset() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to request slave reset %x", ret);
                    break;
                }
            }
        } while (ret == ESP_ERR_NOT_FINISHED);
    }
    xSemaphoreGive(s_essl_mutex);
    return ESP_OK;
}

void eppp_sdio_host_deinit(void)
{
    essl_sdio_deinit_dev(s_essl);
    sdmmc_host_deinit();
    free(s_card);
    s_card = NULL;
    s_essl = NULL;
}

#else // SDMMC_HOST NOT-SUPPORTED

esp_err_t eppp_sdio_host_tx(void *handle, void *buffer, size_t len)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t eppp_sdio_host_rx(esp_netif_t *netif)
{
    return ESP_ERR_NOT_SUPPORTED;
}

void eppp_sdio_host_deinit(void)
{
}

esp_err_t eppp_sdio_host_init(struct eppp_config_sdio_s *config)
{
    return ESP_ERR_NOT_SUPPORTED;
}
#endif // CONFIG_SOC_SDIO_SLAVE_SUPPORTED
