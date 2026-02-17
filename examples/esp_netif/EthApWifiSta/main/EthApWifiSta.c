/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Ethernet AP + WiFi STA Example
 *
 * This example initializes an Ethernet interface as an Access Point (with DHCP server)
 * and a WiFi interface as a Station. The Ethernet AP provides network access to
 * connected devices while the WiFi STA connects to an external access point.
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
#include "lwip/inet.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ethernet_init.h"
#include "sdkconfig.h"

static const char *TAG = "eth_ap_wifi_sta";

static EventGroupHandle_t event_group = NULL;

/** Event handler for Ethernet events */
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

/** Event handler for WiFi events */
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
            ESP_LOGI(TAG, "Wi-Fi STA disconnected, retrying...");
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

    // Print DNS information
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        ESP_LOGI(TAG, "DHCP_DNS_MAIN:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    }
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK) {
        ESP_LOGI(TAG, "DHCP_DNS_BACKUP:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    }

    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

/** Initialize WiFi STA */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_EXAMPLE_ESP_WIFI_SSID,
            .password = CONFIG_EXAMPLE_ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA initialized. SSID:%s password:%s", CONFIG_EXAMPLE_ESP_WIFI_SSID, CONFIG_EXAMPLE_ESP_WIFI_PASS);
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

    event_group = xEventGroupCreate();

    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles = NULL;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    if (eth_port_cnt == 0) {
        ESP_LOGE(TAG, "No Ethernet interface initialized");
        return;
    }

    // Static IP configuration for Ethernet AP
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
        // Set DNS server addresses (will be updated from WiFi STA DNS if available)
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
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));

    ESP_LOGI(TAG, "Ethernet AP initialized. AP IP: " IPSTR ", netmask: " IPSTR,
             IP2STR(&s_ap_ip_info.ip), IP2STR(&s_ap_ip_info.netmask));
    ESP_LOGI(TAG, "Connect a device to the Ethernet port to get an IP via DHCP");

    // Initialize WiFi STA
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(sta_netif != NULL ? ESP_OK : ESP_FAIL);
    wifi_init_sta();

    // Wait for WiFi STA to get IP address
    xEventGroupWaitBits(event_group, 1, pdTRUE, pdTRUE, portMAX_DELAY);

    // Update Ethernet AP DNS with WiFi STA DNS if enabled
#if CONFIG_EXAMPLE_ETH_AP_DHCP_ENABLE_DNS
    esp_netif_dns_info_t sta_dns_info;
    if (esp_netif_get_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &sta_dns_info) == ESP_OK) {
        esp_netif_dns_info_t eth_dns_info;
        memset(&eth_dns_info, 0, sizeof(esp_netif_dns_info_t));
        eth_dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        eth_dns_info.ip.u_addr.ip4 = sta_dns_info.ip.u_addr.ip4;
        err = esp_netif_set_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &eth_dns_info);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Updated Ethernet AP DNS with WiFi STA DNS: " IPSTR, IP2STR(&sta_dns_info.ip.u_addr.ip4));
        }
    }
#endif // CONFIG_EXAMPLE_ETH_AP_DHCP_ENABLE_DNS

#if CONFIG_LWIP_IPV4_NAPT
    // Setup NAPT (Network Address Port Translation) if enabled
    ESP_ERROR_CHECK(esp_netif_napt_enable(eth_netif));
    ESP_LOGI(TAG, "NAPT enabled on Ethernet AP");
#endif
}
