/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* Multiple Network Interface Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <lwip/dns.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "iface_info.h"

iface_info_t *setup_eth(int prio);
iface_info_t *setup_wifi(int prio);
iface_info_t *setup_ppp(int prio);
esp_err_t check_connectivity(const char *host);

#define HOST        "www.espressif.com"
#define ETH_PRIO    200
#define WIFI_PRIO   100
#define PPP_PRIO    50

static const char *TAG = "app_main";

static ssize_t get_default(iface_info_t *list[], size_t num)
{
    esp_netif_t *default_netif = esp_netif_get_default_netif();
    if (default_netif == NULL) {
        ESP_LOGE(TAG, "default netif is NULL!");
        return -1;
    }
    ESP_LOGI(TAG, "Default netif: %s", esp_netif_get_desc(default_netif));

    for (int i = 0; i < num; ++i) {
        if (list[i] && list[i]->netif == default_netif) {
            ESP_LOGI(TAG, "Default interface: %s", list[i]->name);
            return i;
        }
    }
    // not found
    return -2;
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
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // all interfaces
    iface_info_t *ifaces[] = {
        setup_eth(ETH_PRIO),
        setup_wifi(WIFI_PRIO),
        setup_ppp(PPP_PRIO),
    };
    size_t num_of_ifaces = sizeof(ifaces) / sizeof(ifaces[0]);

    while (true) {
        dns_clear_cache();
        vTaskDelay(pdMS_TO_TICKS(2000));
        ssize_t i = get_default(ifaces, num_of_ifaces);
        if (i == -1) {  // default netif is NULL, probably all interfaces are down -> retry
            continue;
        } else if (i < 0) {
            break;      // some other error, exit
        }

        esp_err_t connect_status = check_connectivity(HOST);
        if (connect_status == ESP_OK) {
            // connectivity ok
            continue;
        }
        if (connect_status == ESP_ERR_NOT_FOUND) {
            // set the default DNS info to global DNS server list
            for (int j = 0; j < 2; ++j) {
                esp_netif_dns_info_t dns_info;
                esp_netif_get_dns_info(ifaces[i]->netif, j, &dns_info);
                if (memcmp(&dns_info.ip, &ifaces[i]->dns[j].ip, sizeof(esp_ip_addr_t)) == 0) {
                    connect_status = ESP_FAIL;
                } else {
                    esp_netif_set_dns_info(ifaces[i]->netif, j, &ifaces[i]->dns[j]);
                    ESP_LOGI(TAG, "Reconfigured DNS%i=" IPSTR, j, IP2STR(&ifaces[i]->dns[j].ip.u_addr.ip4));
                }
            }
        }
        if (connect_status == ESP_FAIL) {
            ESP_LOGE(TAG, "No connection via the default netif!");
            // try to switch interfaces manually
            // WARNING: Once we set_default_netif() manually, we disable the automatic prio-routing
            int next = (i + 1) % num_of_ifaces;
            while (ifaces[i] != ifaces[next]) {
                if (ifaces[next]->connected) {
                    ESP_LOGE(TAG, "Trying another interface: %s", ifaces[next]->name);
                    esp_netif_set_default_netif(ifaces[next]->netif);
                    break;
                }
                ++next;
                next = next % num_of_ifaces;
            }
        }
    }

    ESP_LOGI(TAG, "Stop and cleanup all interfaces");
    for (int i = 0; i < num_of_ifaces; ++i) {
        if (ifaces[i]) {
            ifaces[i]->teardown(ifaces[i]);
        }
    }
}
