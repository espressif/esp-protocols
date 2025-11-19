/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Ethernet Basic Initialization

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "iface_info.h"
#include "esp_idf_version.h"

static const char *TAG = "ethernet_connect";

struct eth_info_t {
    iface_info_t parent;
    esp_eth_handle_t eth_handle;
    esp_eth_netif_glue_handle_t glue;
    esp_eth_mac_t *mac;
    esp_eth_phy_t *phy;
};

static void eth_event_handler(void *args, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    struct eth_info_t *eth_info = args;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        eth_info->parent.connected = true;
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        eth_info->parent.connected = false;
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

static void got_ip_event_handler(void *args, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    struct eth_info_t *eth_info = args;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    for (int i = 0; i < 2; ++i) {
        esp_netif_get_dns_info(eth_info->parent.netif, i, &eth_info->parent.dns[i]);
        ESP_LOGI(TAG, "DNS %i:" IPSTR, i, IP2STR(&eth_info->parent.dns[i].ip.u_addr.ip4));
    }
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}


static void eth_destroy(iface_info_t *info)
{
    struct eth_info_t *eth_info = __containerof(info, struct eth_info_t, parent);

    esp_eth_stop(eth_info->eth_handle);
    esp_eth_del_netif_glue(eth_info->glue);
    esp_eth_driver_uninstall(eth_info->eth_handle);
    eth_info->phy->del(eth_info->phy);
    eth_info->mac->del(eth_info->mac);
    esp_netif_destroy(eth_info->parent.netif);
    free(eth_info);
}

iface_info_t *example_eth_init(int prio)
{
    struct eth_info_t *eth_info = malloc(sizeof(struct eth_info_t));
    assert(eth_info);
    eth_info->parent.destroy = eth_destroy;
    eth_info->parent.name = "Ethernet";

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Use internal ESP32's ethernet
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_info->mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    eth_info->phy = esp_eth_phy_new_generic(&phy_config);
#else
    eth_info->phy = esp_eth_phy_new_ip101(&phy_config);
#endif
    // Init Ethernet driver to default and install it
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(eth_info->mac, eth_info->phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_info->eth_handle));

    // Create an instance of esp-netif for Ethernet
    esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base_netif_cfg.route_prio = prio;
    esp_netif_config_t cfg =     {
        .base = &base_netif_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    eth_info->parent.netif = esp_netif_new(&cfg);
    eth_info->glue = esp_eth_new_netif_glue(eth_info->eth_handle);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_info->parent.netif, eth_info->glue));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, eth_info));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, eth_info));

    // Start Ethernet driver state machine
    ESP_ERROR_CHECK(esp_eth_start(eth_info->eth_handle));

    return &eth_info->parent;
}
