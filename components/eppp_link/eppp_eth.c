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
#include "esp_eth_driver.h"
#include "ethernet_init.h"
#include "esp_eth_spec.h"

typedef struct header {
    uint8_t dst[ETH_ADDR_LEN];
    uint8_t src[ETH_ADDR_LEN];
    uint16_t len;
} __attribute__((packed)) header_t;

static const char *TAG = "eppp_ethernet";
static bool s_is_connected = false;
static esp_eth_handle_t *s_eth_handles = NULL;
static uint8_t s_out_buffer[ETH_MAX_PACKET_SIZE];
static uint8_t s_their_mac[ETH_ADDR_LEN];
static uint8_t s_our_mac[ETH_ADDR_LEN];

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        s_is_connected = true;
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        s_is_connected = false;
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static esp_err_t receive(esp_eth_handle_t h, uint8_t *buffer, uint32_t len, void *netif)
{
    header_t *head = (header_t *)buffer;
    size_t packet_len = head->len;
    if (len >= packet_len) {
        esp_err_t ret = esp_netif_receive(netif, buffer + ETH_HEADER_LEN, packet_len, NULL);
        free(buffer);
        return ret;
    }
    return ESP_FAIL;
}

__attribute__((weak)) esp_err_t eppp_transport_ethernet_init(esp_eth_handle_t *handle_array[])
{
    uint8_t eth_port_cnt = 0;
    ESP_RETURN_ON_ERROR(ethernet_init_all(handle_array, &eth_port_cnt), TAG, "Failed to init common eth drivers");
    ESP_RETURN_ON_FALSE(eth_port_cnt == 1, ESP_ERR_INVALID_ARG, TAG, "multiple Ethernet devices detected, please init only one");
    return ESP_OK;
}

__attribute__((weak)) void eppp_transport_ethernet_deinit(esp_eth_handle_t *handle_array)
{
    ethernet_deinit_all(s_eth_handles);
}


esp_err_t eppp_transport_init(eppp_config_t *config, esp_netif_t *esp_netif)
{
    ESP_RETURN_ON_ERROR(eppp_transport_ethernet_init(&s_eth_handles), TAG, "Failed to initialize Ethernet driver");
    ESP_RETURN_ON_ERROR(esp_eth_update_input_path(s_eth_handles[0], receive, esp_netif), TAG, "Failed to set Ethernet Rx callback");
    sscanf(CONFIG_EPPP_LINK_ETHERNET_OUR_ADDRESS, "%2" PRIu8 ":%2" PRIu8 ":%2" PRIi8 ":%2" PRIu8 ":%2" PRIu8 ":%2" PRIu8,
           &s_our_mac[0], &s_our_mac[1], &s_our_mac[2], &s_our_mac[3], &s_our_mac[4], &s_our_mac[5]);

    sscanf(CONFIG_EPPP_LINK_ETHERNET_THEIR_ADDRESS, "%2" PRIu8 ":%2" PRIu8 ":%2" PRIi8 ":%2" PRIu8 ":%2" PRIu8 ":%2" PRIu8,
           &s_their_mac[0], &s_their_mac[1], &s_their_mac[2], &s_their_mac[3], &s_their_mac[4], &s_their_mac[5]);
    esp_eth_ioctl(s_eth_handles[0], ETH_CMD_S_MAC_ADDR, s_our_mac);
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL), TAG, "Failed to register Ethernet handlers");
    ESP_RETURN_ON_ERROR(esp_eth_start(s_eth_handles[0]), TAG, "Failed to start Ethernet driver");
    return ESP_OK;
}

void eppp_transport_deinit(void)
{
    esp_eth_stop(s_eth_handles[0]);
    eppp_transport_ethernet_deinit(s_eth_handles);
}

esp_err_t eppp_transport_tx(void *h, void *buffer, size_t len)
{
    if (!s_is_connected) {
        return ESP_FAIL;
    }
    // setup Ethernet header
    header_t *head = (header_t *)s_out_buffer;
    memcpy(head->dst, s_their_mac, ETH_ADDR_LEN);
    memcpy(head->src, s_our_mac, ETH_ADDR_LEN);
    head->len = len;
    // support only payloads with len <=  ETH_MAX_PAYLOAD_LEN
    if (len > ETH_MAX_PAYLOAD_LEN) {
        return ESP_FAIL;
    }
    memcpy(s_out_buffer + ETH_HEADER_LEN, buffer, len);
    return esp_eth_transmit(s_eth_handles[0], s_out_buffer, len + ETH_HEADER_LEN);
}
