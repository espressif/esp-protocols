/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * Ethernet AP + WiFi AP Example
 *
 * This example initializes both Ethernet and WiFi interfaces as Access Points (with DHCP servers).
 * Both interfaces provide network access to connected devices simultaneously.
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

static const char *TAG = "eth_ap_wifi_ap";

/* Common/Utility functions */

/** Parse IP address configuration from strings into ip_info structure */
static esp_err_t parse_ip_config_from_strings(esp_netif_ip_info_t *ip_info,
                                              const char *ip_addr, const char *netmask, const char *gw)
{
    memset(ip_info, 0, sizeof(esp_netif_ip_info_t));

    if (!inet_aton(ip_addr, &ip_info->ip)) {
        ESP_LOGE(TAG, "Invalid IP address: %s", ip_addr);
        return ESP_ERR_INVALID_ARG;
    }
    if (!inet_aton(netmask, &ip_info->netmask)) {
        ESP_LOGE(TAG, "Invalid netmask: %s", netmask);
        return ESP_ERR_INVALID_ARG;
    }
    if (!inet_aton(gw, &ip_info->gw)) {
        ESP_LOGE(TAG, "Invalid gateway: %s", gw);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/** Configure DHCP server options (lease time and IP pool) */
static esp_err_t configure_dhcp_server_options(esp_netif_t *netif, uint32_t lease_time,
                                               const char *start_addr, const char *end_addr)
{
    esp_err_t err;

    // Set lease time
    err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(lease_time));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set DHCP lease time: %s", esp_err_to_name(err));
    }

    // Configure IP address pool
    dhcps_lease_t dhcp_lease;
    memset(&dhcp_lease, 0, sizeof(dhcps_lease_t));
    dhcp_lease.enable = true;
    if (!inet_aton(start_addr, &dhcp_lease.start_ip)) {
        ESP_LOGW(TAG, "Invalid DHCP start address: %s, using default pool", start_addr);
    } else if (!inet_aton(end_addr, &dhcp_lease.end_ip)) {
        ESP_LOGW(TAG, "Invalid DHCP end address: %s, using default pool", end_addr);
    } else {
        err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, REQUESTED_IP_ADDRESS, &dhcp_lease, sizeof(dhcps_lease_t));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set DHCP IP pool: %s", esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

/** Configure DHCP DNS servers for a network interface */
static esp_err_t configure_dhcp_dns(esp_netif_t *netif, const char *dns_main, const char *dns_backup)
{
    esp_err_t err;

    // Enable DNS in DHCP offers
    dhcps_offer_t dhcps_dns_value = OFFER_DNS;
    err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable DNS in DHCP offers: %s", esp_err_to_name(err));
        return err;
    }

    // Set DNS server addresses
    esp_netif_dns_info_t dns_info;
    memset(&dns_info, 0, sizeof(esp_netif_dns_info_t));
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;

    if (dns_main && inet_aton(dns_main, &dns_info.ip.u_addr.ip4)) {
        err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set primary DNS: %s", esp_err_to_name(err));
        }
    }

    if (dns_backup && inet_aton(dns_backup, &dns_info.ip.u_addr.ip4)) {
        err = esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set backup DNS: %s", esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

/** Print IP information for a network interface */
static void print_ip_info(esp_netif_t *netif, const char *interface_name)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_dns_info_t dns_info;

    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "%s Got IP Address", interface_name);
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip_info.gw));

        // Print DNS information
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
            ESP_LOGI(TAG, "DHCP_DNS_MAIN:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK) {
            ESP_LOGI(TAG, "DHCP_DNS_BACKUP:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }

        ESP_LOGI(TAG, "~~~~~~~~~~~");
    }
}

/* Ethernet-related functions */

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


/**
 * Configure Ethernet AP network interface (netif) layer
 *
 * This function sets up the L3 (network layer) configuration for the Ethernet AP:
 * - Parses IP configuration (IP address, netmask, gateway)
 * - Creates the Ethernet AP netif with inherent configuration
 * - Configures DHCP server options (lease time, IP pool)
 * - Configures DNS servers (if enabled)
 * - Starts the DHCP server
 *
 * @return Pointer to the configured Ethernet AP netif, or NULL on error
 */
static esp_netif_t *eth_ap_setup_netif(void)
{
    // Static IP configuration for Ethernet AP
    static esp_netif_ip_info_t s_eth_ap_ip_info;
    esp_err_t err = parse_ip_config_from_strings(&s_eth_ap_ip_info,
                                                 CONFIG_EXAMPLE_ETH_AP_IP_ADDR,
                                                 CONFIG_EXAMPLE_ETH_AP_NETMASK,
                                                 CONFIG_EXAMPLE_ETH_AP_GW);
    if (err != ESP_OK) {
        return NULL;
    }

    static esp_netif_inherent_config_t inherent_config = {
        .flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP),
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)
        .ip_info = &s_eth_ap_ip_info,
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
    configure_dhcp_server_options(eth_netif,
                                  CONFIG_EXAMPLE_ETH_AP_DHCP_LEASE_TIME,
                                  CONFIG_EXAMPLE_ETH_AP_DHCP_START_ADDR,
                                  CONFIG_EXAMPLE_ETH_AP_DHCP_END_ADDR);

    // Configure DNS servers if enabled
#if CONFIG_EXAMPLE_ETH_AP_DHCP_ENABLE_DNS
    configure_dhcp_dns(eth_netif, CONFIG_EXAMPLE_ETH_AP_DHCP_DNS_MAIN, CONFIG_EXAMPLE_ETH_AP_DHCP_DNS_BACKUP);
#endif // CONFIG_EXAMPLE_ETH_AP_DHCP_ENABLE_DNS

    ESP_ERROR_CHECK(esp_netif_dhcps_start(eth_netif));

    ESP_LOGI(TAG, "Ethernet AP netif configured. AP IP: " IPSTR ", netmask: " IPSTR,
             IP2STR(&s_eth_ap_ip_info.ip), IP2STR(&s_eth_ap_ip_info.netmask));

    return eth_netif;
}

/**
 * Initialize and start Ethernet AP
 *
 * This is the main entry point for Ethernet AP initialization. It handles:
 * - Registering Ethernet event handlers (L2 events)
 * - Attaching the netif to the Ethernet driver handle
 * - Starting the Ethernet driver
 *
 * @param eth_handle Ethernet driver handle (must be initialized via ethernet_init_all())
 * @return Pointer to the Ethernet AP netif, or NULL on error
 */
static esp_netif_t *eth_ap_start(esp_eth_handle_t eth_handle)
{
    // Register Ethernet event handlers - register before netif setup to catch any events during setup
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));

    // Setup Ethernet AP network interface (IP, DHCP, DNS configuration)
    esp_netif_t *eth_netif = eth_ap_setup_netif();
    if (eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed to setup Ethernet AP netif");
        return NULL;
    }

    // Attach netif to Ethernet driver and start Ethernet
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    // Print Ethernet AP IP information
    print_ip_info(eth_netif, "Ethernet");
    ESP_LOGI(TAG, "Ethernet AP started. Connect a device to the Ethernet port to get an IP via DHCP");

    return eth_netif;
}

/* WiFi-related functions */

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
    }
}

/**
 * Configure WiFi AP network interface (netif) layer
 *
 * This function sets up the L3 (network layer) configuration for the WiFi AP:
 * - Creates the WiFi AP netif
 * - Configures static IP address, netmask, and gateway
 * - Configures DHCP server options (lease time, IP pool)
 * - Configures DNS servers (if enabled)
 * - Starts the DHCP server
 *
 * @return Pointer to the configured WiFi AP netif, or NULL on error
 */
static esp_netif_t *wifi_ap_setup_netif(void)
{
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(ap_netif != NULL ? ESP_OK : ESP_FAIL);

    // Static IP configuration for WiFi AP
    static esp_netif_ip_info_t s_ap_ip_info;
    esp_err_t err = parse_ip_config_from_strings(&s_ap_ip_info,
                                                 CONFIG_EXAMPLE_WIFI_AP_IP_ADDR,
                                                 CONFIG_EXAMPLE_WIFI_AP_NETMASK,
                                                 CONFIG_EXAMPLE_WIFI_AP_GW);
    if (err != ESP_OK) {
        return NULL;
    }

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &s_ap_ip_info));

    /* Configure DHCP server options */
    configure_dhcp_server_options(ap_netif,
                                  CONFIG_EXAMPLE_WIFI_AP_DHCP_LEASE_TIME,
                                  CONFIG_EXAMPLE_WIFI_AP_DHCP_START_ADDR,
                                  CONFIG_EXAMPLE_WIFI_AP_DHCP_END_ADDR);

    // Configure DNS servers if enabled
#if CONFIG_EXAMPLE_WIFI_AP_DHCP_ENABLE_DNS
    configure_dhcp_dns(ap_netif, CONFIG_EXAMPLE_WIFI_AP_DHCP_DNS_MAIN, CONFIG_EXAMPLE_WIFI_AP_DHCP_DNS_BACKUP);
#endif // CONFIG_EXAMPLE_WIFI_AP_DHCP_ENABLE_DNS

    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    ESP_LOGI(TAG, "Wi-Fi AP initialized.");

    return ap_netif;
}

/**
 * Initialize and start WiFi AP
 *
 * This is the main entry point for WiFi AP initialization. It handles:
 * - Registering WiFi AP event handlers (L2 events)
 * - Setting up the network interface via wifi_ap_setup_netif() (L3 configuration)
 * - Initializing the WiFi driver
 * - Configuring WiFi AP settings (SSID, password, channel, authentication)
 * - Starting the WiFi AP
 *
 * @return Pointer to the WiFi AP netif, or NULL on error
 */
static esp_netif_t *wifi_ap_start(void)
{
    // Register WiFi AP event handlers - register only specific events to avoid warnings for unhandled events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, wifi_ap_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, wifi_ap_event_handler, NULL));

    // Setup WiFi AP network interface (IP, DHCP, DNS configuration)
    esp_netif_t *wifi_ap_netif = wifi_ap_setup_netif();
    if (wifi_ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to setup WiFi AP netif");
        return NULL;
    }

    // Initialize WiFi driver
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Configure WiFi AP settings (SSID, password, channel, authentication)
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

    // Set mode and start WiFi AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID:%s password:%s channel:%d",
             CONFIG_EXAMPLE_WIFI_AP_SSID, CONFIG_EXAMPLE_WIFI_AP_PASS, CONFIG_EXAMPLE_WIFI_AP_CHANNEL);

    // Print WiFi AP IP information
    print_ip_info(wifi_ap_netif, "Wi-Fi AP");
    ESP_LOGI(TAG, "Wifi AP started. Connect a device to the Wi-Fi AP to get an IP via DHCP");

    return wifi_ap_netif;
}

/* Main function */

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

    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles = NULL;
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    if (eth_port_cnt == 0) {
        ESP_LOGE(TAG, "No Ethernet interface initialized");
        return;
    }

    // Initialize Ethernet AP
    esp_netif_t *eth_netif = eth_ap_start(eth_handles[0]);
    if (eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize Ethernet AP");
        return;
    }

    // Initialize WiFi AP
    esp_netif_t *wifi_ap_netif = wifi_ap_start();
    if (wifi_ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP");
        return;
    }

#if CONFIG_LWIP_IPV4_NAPT
    // Setup NAPT (Network Address Port Translation) if enabled
    ESP_ERROR_CHECK(esp_netif_napt_enable(eth_netif));
    ESP_LOGI(TAG, "NAPT enabled on Ethernet AP");
    ESP_ERROR_CHECK(esp_netif_napt_enable(wifi_ap_netif));
    ESP_LOGI(TAG, "NAPT enabled on Wi-Fi AP");
#endif
}
