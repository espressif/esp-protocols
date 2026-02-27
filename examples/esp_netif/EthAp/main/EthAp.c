/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Ethernet AP Example
 *
 * This example initializes a single Ethernet interface and runs a DHCP server
 * on it, turning it into an Ethernet Access Point. Devices connecting to the
 * Ethernet port will receive an IP address via DHCP (e.g. 192.168.5.x).
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ethernet_init.h"

static const char *TAG = "eth_ap";

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
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

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles = NULL;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    if (eth_port_cnt == 0) {
        ESP_LOGE(TAG, "No Ethernet interface initialized");
        return;
    }

    /* Static IP configuration for Ethernet AP */
    static esp_netif_ip_info_t s_ap_ip_info;
    memset(&s_ap_ip_info, 0, sizeof(esp_netif_ip_info_t));

    if (!inet_aton(CONFIG_EXAMPLE_ETH_AP_IP_ADDR, &s_ap_ip_info.ip)) {
        ESP_LOGE(TAG, "Invalid IP address: %s", CONFIG_EXAMPLE_ETH_AP_IP_ADDR);
        return;
    }
    if (!inet_aton(CONFIG_EXAMPLE_ETH_AP_NETMASK, &s_ap_ip_info.netmask)) {
        ESP_LOGE(TAG, "Invalid netmask: %s", CONFIG_EXAMPLE_ETH_AP_NETMASK);
        return;
    }
    if (!inet_aton(CONFIG_EXAMPLE_ETH_AP_GW, &s_ap_ip_info.gw)) {
        ESP_LOGE(TAG, "Invalid gateway: %s", CONFIG_EXAMPLE_ETH_AP_GW);
        return;
    }

    static esp_netif_inherent_config_t inherent_config = {
        .flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP),
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)
        .ip_info = &s_ap_ip_info,
        .get_ip_event = 0,
        .lost_ip_event = 0,
        .if_key = "ETH_AP",
        .if_desc = "eth_ap",
        .route_prio = 50,
        .bridge_info = NULL
    };

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    cfg.base = &inherent_config;

    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    ESP_ERROR_CHECK(eth_netif != NULL ? ESP_OK : ESP_FAIL);

    /* Configure DHCP server options */
    // Set lease time
    uint32_t lease_time = CONFIG_EXAMPLE_ETH_AP_DHCP_LEASE_TIME;
    esp_err_t err = esp_netif_dhcps_option(eth_netif, ESP_NETIF_OP_SET, IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(lease_time));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set DHCP lease time: %s", esp_err_to_name(err));
    }

    // Configure IP address pool
    dhcps_lease_t dhcp_lease;
    memset(&dhcp_lease, 0, sizeof(dhcps_lease_t));
    dhcp_lease.enable = true;
    if (!inet_aton(CONFIG_EXAMPLE_ETH_AP_DHCP_START_ADDR, &dhcp_lease.start_ip)) {
        ESP_LOGW(TAG, "Invalid DHCP start address: %s, using default pool", CONFIG_EXAMPLE_ETH_AP_DHCP_START_ADDR);
    } else if (!inet_aton(CONFIG_EXAMPLE_ETH_AP_DHCP_END_ADDR, &dhcp_lease.end_ip)) {
        ESP_LOGW(TAG, "Invalid DHCP end address: %s, using default pool", CONFIG_EXAMPLE_ETH_AP_DHCP_END_ADDR);
    } else {
        err = esp_netif_dhcps_option(eth_netif, ESP_NETIF_OP_SET, REQUESTED_IP_ADDRESS, &dhcp_lease, sizeof(dhcps_lease_t));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set DHCP IP pool: %s", esp_err_to_name(err));
        }
    }

    // Configure DNS servers if enabled
#if CONFIG_EXAMPLE_ETH_AP_DHCP_ENABLE_DNS
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    err = esp_netif_dhcps_option(eth_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable DNS in DHCP offers: %s", esp_err_to_name(err));
    } else {
        // Set DNS server addresses
        esp_netif_dns_info_t dns_info;
        memset(&dns_info, 0, sizeof(esp_netif_dns_info_t));
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;

        if (inet_aton(CONFIG_EXAMPLE_ETH_AP_DHCP_DNS_MAIN, &dns_info.ip.u_addr.ip4)) {
            err = esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &dns_info);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set primary DNS: %s", esp_err_to_name(err));
            }
        }

        if (inet_aton(CONFIG_EXAMPLE_ETH_AP_DHCP_DNS_BACKUP, &dns_info.ip.u_addr.ip4)) {
            err = esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set backup DNS: %s", esp_err_to_name(err));
            }
        }
    }
#endif // CONFIG_EXAMPLE_ETH_AP_DHCP_ENABLE_DNS

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, eth_netif));

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));

    ESP_LOGI(TAG, "Ethernet AP initialized. AP IP: " IPSTR ", netmask: " IPSTR,
             IP2STR(&s_ap_ip_info.ip), IP2STR(&s_ap_ip_info.netmask));
    ESP_LOGI(TAG, "Connect a device to the Ethernet port to get an IP via DHCP");
}
