//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _HOST_ESP_NETIF_H_
#define _HOST_ESP_NETIF_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_netif_ip_addr.h"
#include "esp_err.h"
#include "esp_event.h"

struct esp_netif_obj {};

typedef struct esp_netif_driver_base_s {
    esp_err_t (*post_attach)(esp_netif_t *netif, void* h);
    esp_netif_t *netif;
} esp_netif_driver_base_t;

struct esp_netif_driver_ifconfig {
    void* handle;
    esp_err_t (*transmit)(void *h, void *buffer, size_t len);
    esp_err_t (*transmit_wrap)(void *h, void *buffer, size_t len, void *netstack_buffer);
    void (*driver_free_rx_buffer)(void *h, void* buffer);
};

typedef struct esp_netif_obj esp_netif_t;

/** @brief Status of DHCP client or DHCP server */
typedef enum {
    ESP_NETIF_DHCP_INIT = 0,    /**< DHCP client/server is in initial state (not yet started) */
    ESP_NETIF_DHCP_STARTED,     /**< DHCP client/server has been started */
    ESP_NETIF_DHCP_STOPPED,     /**< DHCP client/server has been stopped */
    ESP_NETIF_DHCP_STATUS_MAX
} esp_netif_dhcp_status_t;


esp_netif_t *esp_netif_get_handle_from_ifkey(const char *if_key);

esp_err_t esp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info);

esp_err_t esp_netif_dhcpc_get_status(esp_netif_t *esp_netif, esp_netif_dhcp_status_t *status);

esp_err_t esp_netif_get_ip6_linklocal(esp_netif_t *esp_netif, esp_ip6_addr_t *if_ip6);
int esp_netif_get_netif_impl_index(esp_netif_t *esp_netif);

void esp_netif_action_connected(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data);
void esp_netif_action_disconnected(void *esp_netif, esp_event_base_t base, int32_t event_id, void *data);

#endif // _HOST_ESP_NETIF_H_
