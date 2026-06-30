/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "eppp_link.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/wifi.h"

#define CHAT_CHANNEL 1
#define WIFI_CHANNEL 2

typedef enum {
    UNKNOWN,
    HELLO,
    START,
    ERROR,
} state_t;

typedef struct context {
    eppp_channel_fn_t transmit;
    EventGroupHandle_t flags;
    state_t state;
    esp_netif_t *eppp;
} context_t;

#define HELLO_BIT BIT0
#define START_BIT BIT1
#define CONNECT_BIT BIT2
#define DISCONNECT_BIT BIT3

#define ALL_BITS (HELLO_BIT | START_BIT | CONNECT_BIT | DISCONNECT_BIT)

static const char *TAG = "eppp_host_example_with_channels";
static context_t *s_eppp_channel_ctx = NULL;

static esp_err_t eppp_receive(esp_netif_t *netif, int nr, void *buffer, size_t len)
{
    context_t *ctx = eppp_get_context(netif);
    if (nr == CHAT_CHANNEL) {
        ESP_LOGI(TAG, "Received channel=%d len=%d %.*s", nr, (int)len, (int)len, (char *)buffer);
        const char hello[] = "Hello server";
        const char mac[] = "Get MAC";
        if (len == sizeof(hello) && memcmp(buffer, hello, len) == 0) {
            if (ctx->state == HELLO) {
                xEventGroupSetBits(ctx->flags, HELLO_BIT);
            } else {
                ctx->state = ERROR;
            }
        } else if (len == sizeof(mac) && memcmp(buffer, mac, 5) == 0) {
            if (ctx->state == HELLO) {
                xEventGroupSetBits(ctx->flags, START_BIT);
            } else {
                ctx->state = ERROR;
            }
        }
    } else if (nr == WIFI_CHANNEL) {
        ESP_LOGD(TAG, "Received WIFI channel=%d len=%d", nr, (int)len);
        ESP_LOG_BUFFER_HEXDUMP("wifi-receive", buffer, len, ESP_LOG_VERBOSE);
        return esp_wifi_internal_tx(WIFI_IF_STA, buffer, len);
    } else {
        ESP_LOGE(TAG, "Incorrect channel number %d", nr);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t wifi_receive(void *buffer, uint16_t len, void *eb)
{
    s_eppp_channel_ctx->transmit(s_eppp_channel_ctx->eppp, WIFI_CHANNEL, buffer, len);
    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    context_t *ctx = arg;
    ESP_LOGI(TAG, "event_handler: event_base=%s event_id=%d", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI start event");
        esp_wifi_connect();
        xEventGroupSetBits(ctx->flags, CONNECT_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "connect to the AP fail");
        xEventGroupSetBits(ctx->flags, DISCONNECT_BIT);
    }
}


static void init_wifi_driver(context_t *ctx)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               event_handler, ctx));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

void station_over_eppp_channel(void *arg)
{
    __attribute__((unused)) esp_err_t ret;
    context_t ctx = {
        .transmit = NULL,
        .flags = NULL,
        .state = UNKNOWN,
        .eppp = (esp_netif_t *)arg
    };
    ESP_GOTO_ON_FALSE(ctx.flags = xEventGroupCreate(), ESP_FAIL, err, TAG, "Failed to create event group");
    ESP_GOTO_ON_ERROR(eppp_add_channels(ctx.eppp, &ctx.transmit, eppp_receive, &ctx), err, TAG, "Failed to add channels");
    ESP_GOTO_ON_FALSE(ctx.transmit, ESP_FAIL, err, TAG, "Channel tx function is not set");
    init_wifi_driver(&ctx);

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(ctx.flags, ALL_BITS, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & HELLO_BIT) {
            ESP_LOGI(TAG, "Hello from client received");
            const char hello[] = "Hello client";
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)hello, sizeof(hello));
        } else if (bits & START_BIT) {
            ctx.state = START;
            ESP_LOGI(TAG, "Starting WIFI");
            uint8_t mac[6];
            if (esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK) {
                ESP_LOGE(TAG, "esp_wifi_get_mac failed");
                ctx.state = ERROR;
                continue;
            }
            char mac_data[5 /* MAC: */ + 6 * 2 /* 6 bytes per char */ + 5 /* : */ + 1 /* \0 */];
            sprintf(mac_data, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ESP_LOGI(TAG, "Sending MAC: %.*s", (int)sizeof(mac_data), mac_data);
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)mac_data, sizeof(mac_data));
            ret = esp_wifi_start();
            ESP_LOGI(TAG, "WIFI start result: %d", ret);
            s_eppp_channel_ctx = &ctx;
            esp_wifi_internal_reg_rxcb(WIFI_IF_STA, wifi_receive);
        } else if (bits & CONNECT_BIT) {
            ESP_LOGI(TAG, "WIFI connected");
            const char connected[] = "Connected";
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)connected, sizeof(connected));
        } else if (bits & DISCONNECT_BIT) {
            const char disconnected[] = "Disconnected";
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)disconnected, sizeof(disconnected));
        } else if (ctx.state != START) {
            ctx.state = HELLO;
            const char up[] = "Server up";
            esp_wifi_disconnect();
            esp_wifi_stop();
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)up, sizeof(up));
        }
    }

err:
    vTaskDelete(NULL);
}
