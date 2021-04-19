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

#include<stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include "esp_netif.h"
#include "esp_err.h"

esp_netif_t *esp_netif_get_handle_from_ifkey(const char *if_key)
{
    return NULL;
}

esp_err_t esp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info)
{
    ESP_IPADDR4_INIT(&ip_info->ip, 1,2,3,4);
    return ESP_OK;
}

esp_err_t esp_netif_dhcpc_get_status(esp_netif_t *esp_netif, esp_netif_dhcp_status_t *status)
{
    return ESP_OK;
}


esp_err_t esp_netif_get_ip6_linklocal(esp_netif_t *esp_netif, esp_ip6_addr_t *if_ip6)
{
    struct ifaddrs *addrs, *tmp;
    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp)
    {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            char addr[64];
            struct sockaddr_in6 *pAddr = (struct sockaddr_in6 *)tmp->ifa_addr;
            inet_ntop(AF_INET6, &pAddr->sin6_addr, addr, sizeof(addr) );
            printf("%s: %s\n", tmp->ifa_name, addr);
            inet6_addr_to_ip6addr(if_ip6, &pAddr->sin6_addr);
        }

        tmp = tmp->ifa_next;
    }

    freeifaddrs(addrs);
    return ESP_OK;
}

int esp_netif_get_netif_impl_index(esp_netif_t *esp_netif)
{
    char * ifname = "enp1s0";
    uint32_t interfaceIndex = if_nametoindex(ifname);
    printf("%s: %d\n", ifname, interfaceIndex);
    return interfaceIndex;
}
