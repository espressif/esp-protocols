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
#include "esp_serial_slave_link/essl_sdio.h"
#include "eppp_sdio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

#if CONFIG_SOC_SDMMC_HOST_SUPPORTED

#define TIMEOUT_MAX   UINT32_MAX

static const char *TAG = "eppp_sdio_host";
static SemaphoreHandle_t s_essl_mutex = NULL;
//static sdmmc_card_t *card;

static CACHE_ALIGNED_ATTR uint8_t send_buffer[SDIO_ALIGN(MAX_PAYLOAD)];
static CACHE_ALIGNED_ATTR uint8_t rcv_buffer[SDIO_ALIGN(MAX_PAYLOAD)];

essl_handle_t eppp_get_essl(void *h);

esp_err_t eppp_sdio_host_tx(void *h, void *buffer, size_t len)
{
    memcpy(send_buffer, buffer, len);
    size_t send_len = SDIO_ALIGN(len);
    if (send_len > len) {
        // pad with SOF's
        memset(&send_buffer[len], 0x7E, send_len - len);
    }
    xSemaphoreTake(s_essl_mutex, portMAX_DELAY);
    esp_err_t ret = essl_send_packet(eppp_get_essl(h), send_buffer, send_len, 50);
    if (ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "slave not ready to receive packet %x", ret);
        xSemaphoreGive(s_essl_mutex);
        return ESP_ERR_NO_MEM;

//        ESP_LOGI(TAG, "send reset to slave...");
//        ret = essl_reset_cnt(eppp_get_essl(h));
//        if (ret != ESP_OK) {
//            ESP_LOGE(TAG, "Failed to reset cnt %x", ret);
//            goto fail;
//        }
//        ret = essl_write_reg(eppp_get_essl(h), 0, REQ_RESET, NULL, TIMEOUT_MAX);
//        if (ret != ESP_OK) {
//            ESP_LOGE(TAG, "Failed to write reg %x", ret);
//            goto fail;
//        }
//        ret = essl_send_slave_intr(eppp_get_essl(h), BIT(0), TIMEOUT_MAX);
//        if (ret != ESP_OK) {
//            ESP_LOGE(TAG, "Failed to send interrupt %x", ret);
//            goto fail;
//        }
//
//        vTaskDelay(500 / portTICK_PERIOD_MS);
//        ret = essl_wait_for_ready(eppp_get_essl(h), TIMEOUT_MAX);
//        if (ret != ESP_OK) {
//            ESP_LOGE(TAG, "Failed to wait till ready %x", ret);
//            goto fail;
//        }
    }
//    ESP_LOGW(TAG, "sending data, size: %d %d", len, send_len);
//    ESP_LOG_BUFFER_HEXDUMP(TAG, send_buffer, send_len, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP(TAG, send_buffer, send_len, ESP_LOG_VERBOSE);
    xSemaphoreGive(s_essl_mutex);
    return ESP_OK;
//fail:
//    return ESP_ERR_NO_MEM;
}

static esp_err_t print_cis_information(sdmmc_card_t *card)
{
    uint8_t cis_buffer[256];
    size_t cis_data_len = 1024; //specify maximum searching range to avoid infinite loop
    esp_err_t ret = ESP_OK;

    ret = sdmmc_io_get_cis_data(card, cis_buffer, 256, &cis_data_len);
    if (ret == ESP_ERR_INVALID_SIZE) {
        int temp_buf_size = cis_data_len;
        uint8_t *temp_buf = malloc(temp_buf_size);
        assert(temp_buf);

        ESP_LOGW(TAG, "CIS data longer than expected, temporary buffer allocated.");
        ret = sdmmc_io_get_cis_data(card, temp_buf, temp_buf_size, &cis_data_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to get CIS data.");
            free(temp_buf);
            return ret;
        }

        sdmmc_io_print_cis_info(temp_buf, cis_data_len, NULL);

        free(temp_buf);
    } else if (ret == ESP_OK) {
        sdmmc_io_print_cis_info(cis_buffer, cis_data_len, NULL);
    } else {
        ESP_LOGE(TAG, "failed to get CIS data.");
        return ret;
    }
    return ESP_OK;
}

//static esp_err_t request_slave_reset(essl_handle_t *h)
//{
//    ESP_LOGI(TAG, "send reset to slave...");
//    esp_err_t ret = essl_reset_cnt(*h);
//    if (ret != ESP_OK) {
//        ESP_LOGE(TAG, "Failed to reset cnt %x", ret);
//        ESP_LOGI(TAG, "try to deinit() and init() again...");
//        essl_sdio_deinit_dev(*h);
//        essl_sdio_config_t ser_config = {
//                .card = card,
//                .recv_buffer_size = SDIO_PAYLOAD,
//        };
//        ret = essl_sdio_init_dev(h, &ser_config);
//        if (ret != ESP_OK || h == NULL) {
//            ESP_LOGE(TAG, "essl_sdio_init_dev failed %d", ret);
//            goto fail;
//        }
//        ret = essl_init(*h, TIMEOUT_MAX);
//        if (ret != ESP_OK) {
//            ESP_LOGE(TAG, "essl_init failed %d", ret);
//            goto fail;
//        }
//
//
//        goto fail;
//    }
//
//    ret = essl_write_reg(*h, SLAVE_REG_REQ, REQ_RESET, NULL, TIMEOUT_MAX);
//    if (ret != ESP_OK) {
//        goto fail;
//    }
//    ret = essl_send_slave_intr(*h, BIT(SLAVE_INTR), TIMEOUT_MAX);
//    if (ret != ESP_OK) {
//        goto fail;
//    }
//
//    vTaskDelay(500 / portTICK_PERIOD_MS);
//    ret = essl_wait_for_ready(*h, TIMEOUT_MAX);
//    if (ret != ESP_OK) {
//        goto fail;
//    }
//    ESP_LOGI(TAG, "slave io ready");
//    return ESP_OK;
//fail:
//    return ESP_FAIL;
//}

esp_err_t eppp_sdio_host_init(essl_handle_t *h)
{
    esp_err_t res;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // initialise SDMMC host
    res = sdmmc_host_init();
    if (res != ESP_OK) {
        return ESP_FAIL;
    }

    // configure SDIO interface and slot
    slot_config.width = 4;
    slot_config.clk = 18;
    slot_config.cmd = 19;
    slot_config.d0  = 49;
    slot_config.d1  = 50;
    slot_config.d2  = 16;
    slot_config.d3  = 17;

    res = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "init SDMMC Host slot %d failed", SDMMC_HOST_SLOT_1);
        return ESP_FAIL;
    }

    sdmmc_host_t config = SDMMC_HOST_DEFAULT();
    config.flags = SDMMC_HOST_FLAG_4BIT;
    config.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_card_t *card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    if (card == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (sdmmc_card_init(&config, card) != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc_card_init failed");
        goto fail;
    }

    // output CIS info from the slave
    sdmmc_card_print_info(stdout, card);

    essl_sdio_config_t ser_config = {
        .card = card,
        .recv_buffer_size = SDIO_PAYLOAD,
    };
    res = essl_sdio_init_dev(h, &ser_config);
    if (res != ESP_OK || h == NULL) {
        ESP_LOGE(TAG, "essl_sdio_init_dev failed %d", res);
        goto fail;
    }
    res = essl_init(*h, TIMEOUT_MAX);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "essl_init failed %d", res);
        goto fail;
    }

    if (print_cis_information(card) != ESP_OK) {
        ESP_LOGE(TAG, "failed to print card info");
        goto fail;
    }
    esp_err_t ret;
    ESP_LOGI(TAG, "send reset to slave...");
    ret = essl_write_reg(*h, SLAVE_REG_REQ, REQ_RESET, NULL, TIMEOUT_MAX);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = essl_send_slave_intr(*h, BIT(SLAVE_INTR), TIMEOUT_MAX);
    if (ret != ESP_OK) {
        goto fail;
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
    ret = essl_wait_for_ready(*h, TIMEOUT_MAX);
    if (ret != ESP_OK) {
        goto fail;
    }
    ESP_LOGI(TAG, "slave io ready");
    s_essl_mutex = xSemaphoreCreateMutex();
    if (s_essl_mutex == NULL) {
        goto fail;
    }
    return ESP_OK;

fail:
    essl_sdio_deinit_dev(*h);
    h = NULL;
    return ESP_FAIL;
}

static esp_err_t get_intr(essl_handle_t handle, uint32_t *out_raw)
{
    esp_err_t ret = ESP_OK;
    ret = essl_wait_int(handle, TIMEOUT_MAX);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = essl_get_intr(handle, out_raw, NULL, TIMEOUT_MAX);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = essl_clear_intr(handle, *out_raw, TIMEOUT_MAX);
    if (ret != ESP_OK) {
        return ret;
    }
    ESP_LOGD(TAG, "intr: %08"PRIX32, *out_raw);
    return ESP_OK;
}

esp_err_t eppp_sdio_host_rx(esp_netif_t *netif, essl_handle_t h)
{
    uint32_t intr;
    esp_err_t err = get_intr(h, &intr);
    if (err == ESP_ERR_TIMEOUT) {
//        vTaskDelay(2);
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to read interrupt register");
        return ESP_FAIL;
    }
    const int wait_ms = 50;
    if (intr & ESSL_SDIO_DEF_ESP32.new_packet_intr_mask) {
        xSemaphoreTake(s_essl_mutex, portMAX_DELAY);
        esp_err_t ret;
        do {
            size_t size_read = MAX_PAYLOAD;
            ret = essl_get_packet(h, rcv_buffer, MAX_PAYLOAD, &size_read, wait_ms);
            if (ret == ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "interrupt but no data can be read");
                break;
            } else if (ret == ESP_OK) {
                ESP_LOGD(TAG, "receive data, size: %d", size_read);
                ESP_LOG_BUFFER_HEXDUMP(TAG, rcv_buffer, size_read, ESP_LOG_VERBOSE);
//                ESP_LOG_BUFFER_HEXDUMP(TAG, rcv_buffer, size_read, ESP_LOG_ERROR);
                esp_netif_receive(netif, rcv_buffer, size_read, NULL);
                break;
            } else {
                ESP_LOGE(TAG, "rx packet error: %08X", ret);
//                if (request_slave_reset(&h) != ESP_OK) {
//                    ESP_LOGE(TAG, "Failed to request slave reset %x", ret);
//                    goto fail;
//                }
            }
//            ESP_LOGD(TAG, "receive data, size: %d", size_read);
//            ESP_LOG_BUFFER_HEXDUMP(TAG, rcv_buffer, size_read, ESP_LOG_VERBOSE);
//            esp_netif_receive(netif, rcv_buffer, size_read, NULL);
//            if (ret == ESP_OK) {
//                break;
//            }
        } while (ret == ESP_ERR_NOT_FINISHED);
//    fail:
        xSemaphoreGive(s_essl_mutex);
    }
    return ESP_OK;
}

#else // SDMMC_HOST NOT-SUPPORTED

esp_err_t eppp_sdio_host_tx(void *handle, void *buffer, size_t len)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t eppp_sdio_host_rx(esp_netif_t *netif, essl_handle_t h)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t eppp_sdio_host_init(essl_handle_t *h)
{
    return ESP_ERR_NOT_SUPPORTED;
}
#endif // CONFIG_SOC_SDIO_SLAVE_SUPPORTED
