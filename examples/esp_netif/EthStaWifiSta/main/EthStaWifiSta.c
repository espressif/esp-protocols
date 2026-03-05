/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "ethernet_init.h"
#include "sdkconfig.h"


#define EXAMPLE_ESP_WIFI_SSID      CONFIG_EXAMPLE_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_EXAMPLE_ESP_WIFI_PASS

static const char *TAG = "sta_to_eth_L3";

static EventGroupHandle_t event_group = NULL;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
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


static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        ESP_LOGI(TAG, "Wi-Fi Event: base=%s, id=%ld", event_base, event_id);
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Wi-Fi STA started");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGI(TAG, "Wi-Fi STA stopped");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Wi-Fi STA connected");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "connect to the AP fail... retrying");
            esp_wifi_connect();
            break;
        default:
            ESP_LOGW(TAG, "Unhandled Wi-Fi event: id=%ld", event_id);
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        esp_netif_t *netif = event->esp_netif;
        esp_netif_dns_info_t dns_info;

        ESP_LOGI(TAG, "Wi-Fi Got IP Address");
        ESP_LOGI(TAG, "Event: base=%s, id=%ld", event_base, event_id);
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "STAIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "STAMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "STAGW:" IPSTR, IP2STR(&ip_info->gw));

        // Print DHCP DNS information
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
            ESP_LOGI(TAG, "DHCP_DNS_MAIN:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK) {
            ESP_LOGI(TAG, "DHCP_DNS_BACKUP:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }

        ESP_LOGI(TAG, "~~~~~~~~~~~");

        xEventGroupSetBits(event_group, 1);
    }
}


/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    esp_netif_t *netif = event->esp_netif;
    esp_netif_dns_info_t dns_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "Event: base=%s, id=%ld", event_base, event_id);
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));

    // Print DHCP DNS information
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        ESP_LOGI(TAG, "DHCP_DNS_MAIN:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    }
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK) {
        ESP_LOGI(TAG, "DHCP_DNS_BACKUP:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    }

    ESP_LOGI(TAG, "~~~~~~~~~~~");
}


static void wifi_init_sta(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA initialized. SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void app_main(void)
{
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;

    event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());       // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_event_loop_create_default());       // Create default event loop that running in background

    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();       // default esp-netif configuration parameters.
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    assert(eth_netif);

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));       // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));

    // Initialize wifi
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_sta();
    xEventGroupWaitBits(event_group, 1, pdTRUE, pdTRUE, portMAX_DELAY);

#if CONFIG_LWIP_IPV4_NAPT
    // Setup NAPT (Network Address Port Translation) if enabled
    ESP_ERROR_CHECK(esp_netif_napt_enable(eth_netif));
    ESP_LOGI(TAG, "NAPT enabled on Ethernet");
#endif
}
