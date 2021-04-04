//
// Created by david on 2/7/21.
//
#include<stdio.h>

#include "esp_netif.h"
#include "esp_err.h"
#include<string.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>

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
//        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET)
//        {
//            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
//            printf("%s: %s\n", tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
//        } else

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
