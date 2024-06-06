/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "esp_netif.h"

static int s_itf;
static esp_netif_t *s_netif;
static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
static const char *TAG = "eppp_usb_dev";

static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    size_t rx_size = 0;
    if (itf != s_itf) {
        // Not our channel
        return;
    }
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, buf, rx_size, ESP_LOG_VERBOSE);
        // pass the received data to the network interface
        esp_netif_receive(s_netif, buf, rx_size, NULL);
    } else {
        ESP_LOGE(TAG, "Read error");
    }
}

static void line_state_changed(int itf, cdcacm_event_t *event)
{
    s_itf = itf; // use this channel for the netif communication
    ESP_LOGI(TAG, "Line state changed on channel %d", itf);
}

esp_err_t eppp_transport_init(esp_netif_t *netif)
{
    ESP_LOGI(TAG, "USB initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    /* the second way to register a callback */
    ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
                        TINYUSB_CDC_ACM_0,
                        CDC_EVENT_LINE_STATE_CHANGED,
                        &line_state_changed));
    s_netif = netif;
    return ESP_OK;
}

esp_err_t eppp_transport_tx(void *h, void *buffer, size_t len)
{
    tinyusb_cdcacm_write_queue(s_itf, buffer, len);
    tinyusb_cdcacm_write_flush(s_itf, 0);
    return ESP_OK;
}


void eppp_transport_deinit(void)
{
    // tiny_usb deinit not supported yet
}
