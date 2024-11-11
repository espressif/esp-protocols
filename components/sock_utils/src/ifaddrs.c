/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <sys/socket.h>
#include "esp_netif.h"
#include "esp_check.h"
#include <stdlib.h>
#include "ifaddrs.h"

static const char *TAG = "sockutls_getifaddr";


static esp_err_t getifaddrs_unsafe(void *ctx)
{
    struct ifaddrs **ifap = ctx;
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs **next_addr = NULL;
    struct sockaddr_in *addr_in = NULL;
    esp_netif_t *netif = NULL;
    char if_name[5];    // lwip netif name buffer (e.g. `st1`, 2 letters + number)
    esp_netif_ip_info_t ip;
    esp_err_t ret = ESP_OK;

    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        ESP_GOTO_ON_FALSE((ifaddr = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs))),
                          ESP_ERR_NO_MEM, err, TAG, "Failed to allocate ifaddr");
        if (next_addr == NULL) {    // the first address -> attach the head
            *ifap = ifaddr;
        } else {
            *next_addr = ifaddr;    // attach next addr
        }
        ESP_GOTO_ON_ERROR(esp_netif_get_netif_impl_name(netif, if_name), err, TAG, "Failed to get netif name");
        ESP_GOTO_ON_FALSE((ifaddr->ifa_name = strdup(if_name)),
                          ESP_ERR_NO_MEM, err, TAG, "Failed to allocate if name");
        ESP_GOTO_ON_FALSE((addr_in = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in))),
                          ESP_ERR_NO_MEM, err, TAG, "Failed to allocate addr_in");
        ifaddr->ifa_addr = (struct sockaddr *)addr_in;
        ESP_GOTO_ON_ERROR(esp_netif_get_ip_info(netif, &ip), err, TAG, "Failed to get netif IP");
        ESP_LOGD(TAG, "IPv4 address: " IPSTR, IP2STR(&ip.ip));
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = ip.ip.addr;
        ifaddr->ifa_flags = esp_netif_is_netif_up(netif) ? IFF_UP : 0;
        next_addr = &ifaddr->ifa_next;
    }
    if (next_addr == NULL) {
        *ifap = NULL;       // no addresses found
    } else {
        *next_addr = NULL;  // terminate the list
    }
    return ret;

err:
    if (next_addr) {
        *next_addr = NULL;
        freeifaddrs(*ifap);
    } else {
        freeifaddrs(ifaddr);
    }
    *ifap = NULL;
    return ret;

}

int getifaddrs(struct ifaddrs **ifap)
{
    if (ifap == NULL) {
        errno = EINVAL;
        return -1; // Invalid argument
    }

    esp_err_t ret = esp_netif_tcpip_exec(getifaddrs_unsafe, ifap);
    switch (ret) {
    case ESP_OK:
        return 0;
    case ESP_ERR_NO_MEM:
        errno = ENOMEM;
        return -1;
    case ESP_ERR_INVALID_ARG:
    case ESP_ERR_ESP_NETIF_INVALID_PARAMS:
        errno = EINVAL;
        return -1;
    default:
    case ESP_FAIL:
        errno = EIO;    // Generic I/O Error
        return  -1;
    }
}

void freeifaddrs(struct ifaddrs *ifa)
{
    while (ifa != NULL) {
        struct ifaddrs *next = ifa->ifa_next;
        free(ifa->ifa_name);
        free(ifa->ifa_addr);
        free(ifa);
        ifa = next;
    }
}
