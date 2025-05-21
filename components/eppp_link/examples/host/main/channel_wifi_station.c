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
#include "esp_wifi_remote.h"

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
#define SERVER_UP_BIT BIT3

#define ALL_BITS (HELLO_BIT | START_BIT | CONNECT_BIT | SERVER_UP_BIT)

static uint8_t s_wifi_mac_addr[6] = { 0 };
static const char *TAG = "eppp_host_example_with_channels";

esp_netif_t* esp_wifi_remote_create_default_sta(void);

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "IP event_handler: event_base=%s event_id=%d", event_base, event_id);
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t esp_wifi_remote_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    if (ifx != WIFI_IF_STA) {
        return ESP_ERR_INVALID_STATE;

    }
    for (int i = 0; i < sizeof(s_wifi_mac_addr); i++) {
        if (s_wifi_mac_addr[i] == 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    memcpy(mac, s_wifi_mac_addr, sizeof(s_wifi_mac_addr));
    return ESP_OK;
}

static esp_err_t eppp_receive(esp_netif_t *netif, int nr, void *buffer, size_t len)
{
    context_t *ctx = eppp_get_context(netif);
    if (nr == CHAT_CHANNEL) {
        ESP_LOGI(TAG, "Received channel=%d len=%d %.*s", nr, (int)len, (int)len, (char *)buffer);
        const char hello[] = "Hello client";
        const char mac[] = "MAC: ";
        const char connected[] = "Connected";
        const char server_up[] = "Server up";
        size_t mac_len = 5 /* MAC: */ + 6 * 2 /* 6 bytes per char */ + 5 /* : */ + 1 /* \0 */;
        if (len == sizeof(server_up) && memcmp(buffer, server_up, len) == 0) {
            if (ctx->state == UNKNOWN) {
                ESP_LOGI(TAG, "Server is up");
                ctx->state = HELLO;
            } else {
                ESP_LOGE(TAG, "Received server up in unexpected state %d", ctx->state);
                ctx->state = ERROR;
            }
            xEventGroupSetBits(ctx->flags, SERVER_UP_BIT);
        } else if (len == sizeof(hello) && memcmp(buffer, hello, len) == 0) {
            if (ctx->state == HELLO) {
                xEventGroupSetBits(ctx->flags, HELLO_BIT);
            } else {
                ESP_LOGE(TAG, "Received hello in unexpected state %d", ctx->state);
                ctx->state = ERROR;
            }
        } else if (len == mac_len && memcmp(buffer, mac, 5) == 0) {
            if (ctx->state == HELLO) {
                uint8_t mac_addr[6];
                sscanf((char *)buffer + 5, "%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8,
                       &mac_addr[0], &mac_addr[1], &mac_addr[2], &mac_addr[3], &mac_addr[4], &mac_addr[5]);
                ESP_LOGI(TAG, "Parsed MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
                memcpy(s_wifi_mac_addr, mac_addr, sizeof(s_wifi_mac_addr));
                xEventGroupSetBits(ctx->flags, START_BIT);
            } else {
                ESP_LOGE(TAG, "Received MAC in unexpected state %d", ctx->state);
                ctx->state = ERROR;
            }
        } else if (len == sizeof(connected) && memcmp(buffer, connected, len) == 0) {
            if (ctx->state == START) {
                xEventGroupSetBits(ctx->flags, CONNECT_BIT);
            } else {
                ESP_LOGE(TAG, "Received connected in unexpected state %d", ctx->state);
                ctx->state = ERROR;
            }
        }
    } else if (nr == WIFI_CHANNEL) {
        ESP_LOGD(TAG, "Received WIFI channel=%d len=%d", nr, (int)len);
        ESP_LOG_BUFFER_HEXDUMP("wifi-receive", buffer, len, ESP_LOG_VERBOSE);
        return esp_wifi_remote_channel_rx(ctx->eppp, buffer, NULL, len);
    } else {
        ESP_LOGE(TAG, "Incorrect channel number %d", nr);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t wifi_transmit(void *h, void *buffer, size_t len)
{
    esp_netif_t *eppp = (esp_netif_t *)h;
    context_t *ctx = eppp_get_context(eppp);
    ESP_LOG_BUFFER_HEXDUMP("wifi-transmit", buffer, len, ESP_LOG_VERBOSE);
    return ctx->transmit(eppp, WIFI_CHANNEL, buffer, len);
}

void esp_netif_destroy_wifi_remote(void *esp_netif);

void station_over_eppp_channel(void *arg)
{
    __attribute__((unused)) esp_err_t ret;
    esp_netif_t *wifi = NULL;
    context_t ctx = {
        .transmit = NULL,
        .flags = NULL,
        .state = UNKNOWN,
        .eppp = (esp_netif_t *)arg
    };
    ESP_GOTO_ON_FALSE(ctx.eppp != NULL, ESP_FAIL, err, TAG, "Incorrect EPPP netif");
    ESP_GOTO_ON_FALSE(ctx.flags = xEventGroupCreate(), ESP_FAIL, err, TAG, "Failed to create event group");
    ESP_GOTO_ON_ERROR(eppp_add_channels(ctx.eppp, &ctx.transmit, eppp_receive, &ctx), err, TAG, "Failed to add channels");
    ESP_GOTO_ON_FALSE(ctx.transmit, ESP_FAIL, err, TAG, "Channel tx function is not set");
    ESP_GOTO_ON_ERROR(esp_wifi_remote_channel_set(WIFI_IF_STA, ctx.eppp, wifi_transmit), err, TAG, "Failed to set wifi channel tx function");
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, &ctx);

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(ctx.flags, ALL_BITS, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & HELLO_BIT) {
            ESP_LOGI(TAG, "Hello done");
            if (wifi == NULL) {
                wifi = esp_wifi_remote_create_default_sta();
            }
            const char command[] = "Get MAC";
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)command, sizeof(command));
        } else if (bits & START_BIT) {
            ctx.state = START;
            ESP_LOGI(TAG, "Starting WIFI");
            esp_event_post(WIFI_REMOTE_EVENT, WIFI_EVENT_STA_START, NULL, 0, 0);
        } else if (bits & CONNECT_BIT) {
            ESP_LOGI(TAG, "WIFI connected");
            esp_event_post(WIFI_REMOTE_EVENT, WIFI_EVENT_STA_CONNECTED, NULL, 0, 0);
        } else if ((bits & SERVER_UP_BIT) == SERVER_UP_BIT || ctx.state != START)  {
            if (ctx.state == ERROR) {
                esp_netif_destroy_wifi_remote(wifi);
                wifi = NULL;
                ESP_LOGI(TAG, "WiFi netif has been destroyed");
            }
            const char hello[] = "Hello server";
            ctx.transmit(ctx.eppp, CHAT_CHANNEL, (void*)hello, sizeof(hello));
            ctx.state = HELLO;
        }
    }

err:
    vTaskDelete(NULL);
}
