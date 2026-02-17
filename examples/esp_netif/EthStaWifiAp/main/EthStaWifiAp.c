/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Ethernet Station + WiFi AP Example
 *
 * This example initializes an Ethernet interface as a Station (DHCP client)
 * and a WiFi interface as an Access Point (with DHCP server). The Ethernet Station
 * connects to an external network while the WiFi AP provides network access to
 * connected devices.
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
#include "esp_wifi_types.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ethernet_init.h"
#include "sdkconfig.h"

static const char *TAG = "eth_sta_wifi_ap";

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

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void eth_got_ip_event_handler(void *arg, esp_event_base_t event_base,
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

    xEventGroupSetBits(event_group, 1);
}

/** Event handler for WiFi AP events */
static void wifi_ap_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        ESP_LOGI(TAG, "Wi-Fi Event: base=%s, id=%ld", event_base, event_id);
        switch (event_id) {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "Wi-Fi AP started");
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "Wi-Fi AP stopped");
            break;
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                     MAC2STR(event->mac), event->aid);
        }
        break;
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                     MAC2STR(event->mac), event->aid);
        }
        break;
        default:
            ESP_LOGW(TAG, "Unhandled Wi-Fi AP event: id=%ld", event_id);
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ASSIGNED_IP_TO_CLIENT) {
        ip_event_assigned_ip_to_client_t* event = (ip_event_assigned_ip_to_client_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi AP assigned IP to client: " IPSTR, IP2STR(&event->ip));
    }
}

/** Print WiFi AP IP information */
static void print_ap_ip_info(esp_netif_t *ap_netif)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_dns_info_t dns_info;

    if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi AP Got IP Address");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "APIP:" IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "APMASK:" IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "APGW:" IPSTR, IP2STR(&ip_info.gw));

        // Print DNS information
        if (esp_netif_get_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
            ESP_LOGI(TAG, "DHCP_DNS_MAIN:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }
        if (esp_netif_get_dns_info(ap_netif, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK) {
            ESP_LOGI(TAG, "DHCP_DNS_BACKUP:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }

        ESP_LOGI(TAG, "~~~~~~~~~~~");
    }
}

/** Initialize WiFi AP */
static esp_netif_t *wifi_init_ap(void)
{
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(ap_netif != NULL ? ESP_OK : ESP_FAIL);

    // Static IP configuration for WiFi AP
    static esp_netif_ip_info_t s_ap_ip_info;
    memset(&s_ap_ip_info, 0, sizeof(esp_netif_ip_info_t));

    if (!inet_aton(CONFIG_EXAMPLE_WIFI_AP_IP_ADDR, &s_ap_ip_info.ip)) {
        ESP_LOGE(TAG, "Invalid IP address: %s", CONFIG_EXAMPLE_WIFI_AP_IP_ADDR);
        return NULL;
    }
    if (!inet_aton(CONFIG_EXAMPLE_WIFI_AP_NETMASK, &s_ap_ip_info.netmask)) {
        ESP_LOGE(TAG, "Invalid netmask: %s", CONFIG_EXAMPLE_WIFI_AP_NETMASK);
        return NULL;
    }
    if (!inet_aton(CONFIG_EXAMPLE_WIFI_AP_GW, &s_ap_ip_info.gw)) {
        ESP_LOGE(TAG, "Invalid gateway: %s", CONFIG_EXAMPLE_WIFI_AP_GW);
        return NULL;
    }

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &s_ap_ip_info));

    /* Configure DHCP server options */
    // Set lease time
    uint32_t lease_time = CONFIG_EXAMPLE_WIFI_AP_DHCP_LEASE_TIME;
    esp_err_t err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(lease_time));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set DHCP lease time: %s", esp_err_to_name(err));
    }

    // Configure IP address pool
    dhcps_lease_t dhcp_lease;
    memset(&dhcp_lease, 0, sizeof(dhcps_lease_t));
    dhcp_lease.enable = true;
    if (!inet_aton(CONFIG_EXAMPLE_WIFI_AP_DHCP_START_ADDR, &dhcp_lease.start_ip)) {
        ESP_LOGW(TAG, "Invalid DHCP start address: %s, using default pool", CONFIG_EXAMPLE_WIFI_AP_DHCP_START_ADDR);
    } else if (!inet_aton(CONFIG_EXAMPLE_WIFI_AP_DHCP_END_ADDR, &dhcp_lease.end_ip)) {
        ESP_LOGW(TAG, "Invalid DHCP end address: %s, using default pool", CONFIG_EXAMPLE_WIFI_AP_DHCP_END_ADDR);
    } else {
        err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, REQUESTED_IP_ADDRESS, &dhcp_lease, sizeof(dhcps_lease_t));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set DHCP IP pool: %s", esp_err_to_name(err));
        }
    }

    // Configure DNS servers if enabled
#if CONFIG_EXAMPLE_WIFI_AP_DHCP_ENABLE_DNS
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    err = esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable DNS in DHCP offers: %s", esp_err_to_name(err));
    } else {
        // Set DNS server addresses (will be updated from Ethernet Station DNS if available)
        esp_netif_dns_info_t dns_info;
        memset(&dns_info, 0, sizeof(esp_netif_dns_info_t));
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;

        if (inet_aton(CONFIG_EXAMPLE_WIFI_AP_DHCP_DNS_MAIN, &dns_info.ip.u_addr.ip4)) {
            err = esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set primary DNS: %s", esp_err_to_name(err));
            }
        }

        if (inet_aton(CONFIG_EXAMPLE_WIFI_AP_DHCP_DNS_BACKUP, &dns_info.ip.u_addr.ip4)) {
            err = esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set backup DNS: %s", esp_err_to_name(err));
            }
        }
    }
#endif // CONFIG_EXAMPLE_WIFI_AP_DHCP_ENABLE_DNS

    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    ESP_LOGI(TAG, "Wi-Fi AP initialized. AP IP: " IPSTR ", netmask: " IPSTR,
             IP2STR(&s_ap_ip_info.ip), IP2STR(&s_ap_ip_info.netmask));
    ESP_LOGI(TAG, "Connect a device to the Wi-Fi AP to get an IP via DHCP");

    // Print AP IP information
    print_ap_ip_info(ap_netif);

    return ap_netif;
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

    // Register Ethernet event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_got_ip_event_handler, NULL));

    // Create Ethernet netif (DHCP client mode)
    esp_netif_config_t eth_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&eth_cfg);
    ESP_ERROR_CHECK(eth_netif != NULL ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));

    ESP_LOGI(TAG, "Ethernet Station initialized, waiting for IP address...");

    // Register WiFi AP event handlers - register only specific events to avoid warnings for unhandled events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT, wifi_ap_event_handler, NULL));

    // Initialize WiFi AP netif
    esp_netif_t *ap_netif = wifi_init_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP");
        return;
    }

    // Initialize WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Configure WiFi AP
    wifi_config_t ap_wifi_config = {
        .ap = {
            .ssid = CONFIG_EXAMPLE_WIFI_AP_SSID,
            .ssid_len = strlen(CONFIG_EXAMPLE_WIFI_AP_SSID),
            .channel = CONFIG_EXAMPLE_WIFI_AP_CHANNEL,
            .password = CONFIG_EXAMPLE_WIFI_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(CONFIG_EXAMPLE_WIFI_AP_PASS) == 0) {
        ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Set mode and start WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID:%s password:%s channel:%d",
             CONFIG_EXAMPLE_WIFI_AP_SSID, CONFIG_EXAMPLE_WIFI_AP_PASS, CONFIG_EXAMPLE_WIFI_AP_CHANNEL);

    // Wait for Ethernet Station to get IP address
    xEventGroupWaitBits(event_group, 1, pdTRUE, pdTRUE, portMAX_DELAY);

    // Update WiFi AP DNS with Ethernet Station DNS if enabled
#if CONFIG_EXAMPLE_WIFI_AP_DHCP_ENABLE_DNS
    esp_netif_dns_info_t eth_dns_info;
    if (esp_netif_get_dns_info(eth_netif, ESP_NETIF_DNS_MAIN, &eth_dns_info) == ESP_OK) {
        esp_netif_dns_info_t ap_dns_info;
        memset(&ap_dns_info, 0, sizeof(esp_netif_dns_info_t));
        ap_dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        ap_dns_info.ip.u_addr.ip4 = eth_dns_info.ip.u_addr.ip4;
        esp_err_t err = esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &ap_dns_info);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Updated Wi-Fi AP DNS with Ethernet Station DNS: " IPSTR, IP2STR(&eth_dns_info.ip.u_addr.ip4));
        }
    }
#endif // CONFIG_EXAMPLE_WIFI_AP_DHCP_ENABLE_DNS

#if CONFIG_LWIP_IPV4_NAPT
    // Setup NAPT (Network Address Port Translation) if enabled
    ESP_ERROR_CHECK(esp_netif_napt_enable(ap_netif));
    ESP_LOGI(TAG, "NAPT enabled on Wi-Fi AP");
#endif
}
