/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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

/**
 * @brief Appends an entry for one address to the list, at *tail, and advances *tail
 *
 * Takes ownership of addr: once linked, the entry is freed with the list by freeifaddrs(), also on error.
 */
static esp_err_t append_ifaddr(struct ifaddrs ***tail, const char *if_name, unsigned int flags, struct sockaddr *addr)
{
    struct ifaddrs *ifaddr = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
    if (ifaddr == NULL) {
        free(addr);
        return ESP_ERR_NO_MEM;
    }
    **tail = ifaddr;
    *tail = &ifaddr->ifa_next;
    ifaddr->ifa_addr = addr;
    ifaddr->ifa_flags = flags;
    ifaddr->ifa_name = strdup(if_name);
    return ifaddr->ifa_name ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t getifaddrs_unsafe(void *ctx)
{
    struct ifaddrs **ifap = ctx;
    struct ifaddrs **tail = ifap;
    esp_netif_t *netif = NULL;
    char if_name[5];    // lwip netif name buffer (e.g. `st1`, 2 letters + number)
    esp_netif_ip_info_t ip;
    esp_err_t ret = ESP_OK;

    *ifap = NULL;
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        ESP_GOTO_ON_ERROR(esp_netif_get_netif_impl_name(netif, if_name), err, TAG, "Failed to get netif name");
        const unsigned int flags = esp_netif_is_netif_up(netif) ? IFF_UP : 0;
        ESP_GOTO_ON_ERROR(esp_netif_get_ip_info(netif, &ip), err, TAG, "Failed to get netif IP");
        ESP_LOGD(TAG, "IPv4 address: " IPSTR, IP2STR(&ip.ip));
        struct sockaddr_in *addr_in = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
        ESP_GOTO_ON_FALSE(addr_in, ESP_ERR_NO_MEM, err, TAG, "Failed to allocate addr_in");
        addr_in->sin_family = AF_INET;
        addr_in->sin_addr.s_addr = ip.ip.addr;
        ESP_GOTO_ON_ERROR(append_ifaddr(&tail, if_name, flags, (struct sockaddr *)addr_in), err, TAG,
                          "Failed to allocate ifaddr");
    }
    return ret;

err:
    freeifaddrs(*ifap);
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
