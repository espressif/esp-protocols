/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

static const char *TAG = "mdns-simple";

static void initialise_mdns(void)
{
    const char *mdns_hostname = "minifritz";
    const char *mdns_instance = "Hristo's Time Capsule";

    mdns_txt_item_t arduTxtData[4] = {
        {"board", "esp32"},
        {"tcp_check", "no"},
        {"ssh_upload", "no"},
        {"auth_upload", "no"}
    };

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    char *winstance = NULL;
    if (asprintf(&winstance, "%s [%02x:%02x:%02x:%02x:%02x:%02x]",
                 mdns_hostname, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) < 0) {
        abort();
    }

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(mdns_hostname));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", mdns_hostname);
    ESP_ERROR_CHECK(mdns_instance_name_set(mdns_instance));

    // Delegate host: 17.17.17.17
    mdns_ip_addr_t addr4 = { 0 };
    addr4.addr.type = ESP_IPADDR_TYPE_V4;
    esp_netif_str_to_ip4("17.17.17.17", &addr4.addr.u_addr.ip4);
    addr4.next = NULL;
    ESP_ERROR_CHECK(mdns_delegate_hostname_add(mdns_hostname, &addr4));
    ESP_ERROR_CHECK(mdns_delegate_hostname_add("megafritz", &addr4));

    // Services and subtypes mirroring the fuzz test setup
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_fritz", "_tcp", 22, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host(NULL, "_fritz", "_tcp", NULL, "_server"));

    ESP_ERROR_CHECK(mdns_service_add(NULL, "_telnet", "_tcp", 22, NULL, 0));

    ESP_ERROR_CHECK(mdns_service_add(NULL, "_workstation", "_tcp", 9, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_instance_name_set("_workstation", "_tcp", winstance));

    ESP_ERROR_CHECK(mdns_service_add(NULL, "_arduino", "_tcp", 3232, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_txt_set("_arduino", "_tcp", arduTxtData, 4));

    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_instance_name_set("_http", "_tcp", "ESP WebServer"));

    ESP_ERROR_CHECK(mdns_service_add(NULL, "_afpovertcp", "_tcp", 548, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_rfb", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_smb", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_adisk", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_airport", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_printer", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_airplay", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_raop", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_uscan", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_uscans", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_ippusb", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_scanner", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_ipp", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_ipps", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_pdl-datastream", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_ptp", "_tcp", 885, NULL, 0));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_sleep-proxy", "_udp", 885, NULL, 0));

    free(winstance);
}

void init_softap(void);

/* these strings match mdns_ip_protocol_t enumeration */
static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};

static void mdns_print_results(mdns_result_t *results)
{
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t;
    while (r) {
        if (r->esp_netif) {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", i++, esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], r->ttl);
        }
        if (r->instance_name) {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) {
            printf("  TXT : [%zu] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) {
                printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
            }
            printf("\n");
        }
        a = r->addr;
        while (a) {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } else {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            a = a->next;
        }
        r = r->next;
    }
}


static void query_mdns_service(const char *service_name, const char *proto)
{
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if (err) {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
}


void app_main(void)
{
    // ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "mDNS Ver: %s", ESP_MDNS_VERSION_NUMBER);

 
    // Use the common connection helper to bring up Wi‑Fi/Ethernet
    init_softap();
    initialise_mdns();
    while (1) {
        query_mdns_service("_fritz", "_tcp");

    }
   // ESP_ERROR_CHECK(example_connect());
}
