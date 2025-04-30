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
#include "esp_idf_version.h"
#include "eppp_link.h"
#include "eppp_transport.h"
#include "esp_eth_driver.h"
#include "esp_eth_spec.h"
#include "eppp_transport_eth.h"
// Use Ethernet Init component if available
// (otherwise use just simple init/deinit with generic MAC/PHY)
#if __has_include("ethernet_init.h")
#define USE_ETHERNET_INIT_COMPONENT
#include "ethernet_init.h"
#endif

typedef struct header {
    uint8_t dst[ETH_ADDR_LEN];
    uint8_t src[ETH_ADDR_LEN];
    uint16_t len;
} __attribute__((packed)) header_t;

static const char *TAG = "eppp_ethernet";
static bool s_is_connected = false;
static esp_eth_handle_t *s_eth_handles = NULL;
static uint8_t s_their_mac[ETH_ADDR_LEN];
static uint8_t s_our_mac[ETH_ADDR_LEN];

#ifndef USE_ETHERNET_INIT_COMPONENT
static esp_eth_handle_t s_handles[1] = { NULL };
static esp_eth_mac_t *s_mac = NULL;
static esp_eth_phy_t *s_phy = NULL;

static void simple_deinit(esp_eth_handle_t *handle_array[])
{
    if (s_handles[0] != NULL) {
        esp_eth_driver_uninstall(s_handles[0]);
        s_handles[0] = NULL;
    }
    if (s_mac != NULL) {
        s_mac->del(s_mac);
        s_mac = NULL;
    }
    if (s_phy != NULL) {
        s_phy->del(s_phy);
        s_phy = NULL;
    }
}

static esp_err_t simple_init(struct eppp_config_ethernet_s *config, esp_eth_handle_t *handle_array[])
{
    esp_err_t ret = ESP_OK;
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdc_num = config->mdc_io;
    esp32_emac_config.smi_gpio.mdio_num = config->mdio_io;
    s_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = config->phy_addr;
    phy_config.reset_gpio_num = config->rst_io;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    s_phy = esp_eth_phy_new_generic(&phy_config);
#else
    s_phy = esp_eth_phy_new_ip101(&phy_config);
#endif
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(s_mac, s_phy);
    ESP_GOTO_ON_ERROR(esp_eth_driver_install(&eth_config, &s_handles[0]), err, TAG, "Ethernet driver install failed");
    *handle_array = s_handles;
    return ESP_OK;
err:
    simple_deinit(handle_array);
    return ret;
}

#endif // USE_ETHERNET_INIT_COMPONENT

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

__attribute__((weak)) esp_err_t eppp_transport_ethernet_init(struct eppp_config_ethernet_s *config, esp_eth_handle_t *handle_array[])
{
#ifdef USE_ETHERNET_INIT_COMPONENT
    uint8_t eth_port_cnt = 0;
    ESP_RETURN_ON_ERROR(ethernet_init_all(handle_array, &eth_port_cnt), TAG, "Failed to init common eth drivers");
    ESP_RETURN_ON_FALSE(eth_port_cnt == 1, ESP_ERR_INVALID_ARG, TAG, "multiple Ethernet devices detected, please init only one");
    return ESP_OK;
#else
    return simple_init(config, handle_array);
#endif
}

__attribute__((weak)) void eppp_transport_ethernet_deinit(esp_eth_handle_t *handle_array[])
{
#ifdef USE_ETHERNET_INIT_COMPONENT
    ethernet_deinit_all(s_eth_handles);
#else
    simple_deinit(handle_array);
#endif
}

esp_err_t eppp_transport_tx(void *h, void *buffer, size_t len)
{
    static uint8_t out_buffer[ETH_HEADER_LEN];
    if (!s_is_connected) {
        return ESP_FAIL;
    }
    // setup Ethernet header
    header_t *head = (header_t *)out_buffer;
    memcpy(head->dst, s_their_mac, ETH_ADDR_LEN);
    memcpy(head->src, s_our_mac, ETH_ADDR_LEN);
    head->len = len;
    // support only payloads with len <=  ETH_MAX_PAYLOAD_LEN
    if (len > ETH_MAX_PAYLOAD_LEN) {
        return ESP_FAIL;
    }
    return esp_eth_transmit_vargs(s_eth_handles[0], 2, out_buffer, ETH_HEADER_LEN, buffer, len);
}

static esp_err_t start_driver(esp_netif_t *esp_netif)
{
    ESP_RETURN_ON_ERROR(esp_eth_update_input_path(s_eth_handles[0], receive, esp_netif), TAG, "Failed to set Ethernet Rx callback");
    sscanf(CONFIG_EPPP_LINK_ETHERNET_OUR_ADDRESS, "%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8,
           &s_our_mac[0], &s_our_mac[1], &s_our_mac[2], &s_our_mac[3], &s_our_mac[4], &s_our_mac[5]);

    sscanf(CONFIG_EPPP_LINK_ETHERNET_THEIR_ADDRESS, "%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8 ":%2" PRIx8,
           &s_their_mac[0], &s_their_mac[1], &s_their_mac[2], &s_their_mac[3], &s_their_mac[4], &s_their_mac[5]);
    esp_eth_ioctl(s_eth_handles[0], ETH_CMD_S_MAC_ADDR, s_our_mac);
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL), TAG, "Failed to register Ethernet handlers");
    ESP_RETURN_ON_ERROR(esp_eth_start(s_eth_handles[0]), TAG, "Failed to start Ethernet driver");
    return ESP_OK;
}

static esp_err_t post_attach(esp_netif_t *esp_netif, void *args)
{
    eppp_transport_handle_t h = (eppp_transport_handle_t)args;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "Transport handle cannot be null");
    h->base.netif = esp_netif;

    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  h,
        .transmit = eppp_transport_tx,
    };

    ESP_RETURN_ON_ERROR(esp_netif_set_driver_config(esp_netif, &driver_ifconfig), TAG, "Failed to set driver config");
    ESP_LOGI(TAG, "EPPP Ethernet transport attached to EPPP netif %s", esp_netif_get_desc(esp_netif));
    ESP_RETURN_ON_ERROR(start_driver(esp_netif), TAG, "Failed to start EPPP ethernet driver");
    ESP_LOGI(TAG, "EPPP Ethernet driver started");
    return ESP_OK;
}

eppp_transport_handle_t eppp_eth_init(struct eppp_config_ethernet_s *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config, NULL, TAG, "Config cannot be null");
    eppp_transport_handle_t h = calloc(1, sizeof(struct eppp_handle));
    ESP_RETURN_ON_FALSE(h, NULL, TAG, "Failed to allocate eppp_handle");
    ESP_GOTO_ON_ERROR(eppp_transport_ethernet_init(config, &s_eth_handles), err, TAG, "Failed to init Ethernet transport");
    h->base.post_attach = post_attach;
    return h;
err:
    return NULL;
}

void eppp_eth_deinit(eppp_transport_handle_t h)
{
    esp_eth_stop(s_eth_handles[0]);
    eppp_transport_ethernet_deinit(&s_eth_handles);
}
