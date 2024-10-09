/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <sys/socket.h>
#include "esp_netif.h"
#include <stdlib.h>
#include "ifaddrs.h"

int getifaddrs(struct ifaddrs **ifap)
{
    if (ifap == NULL) {
        return -1; // Invalid argument
    }

    // Allocate memory for a single ifaddrs structure
    struct ifaddrs *ifaddr = (struct ifaddrs *)calloc(1, sizeof(struct ifaddrs));
    if (ifaddr == NULL) {
        return -1; // Memory allocation failure
    }

    // Allocate memory for the interface name
    ifaddr->ifa_name = strdup("sta"); // Replace with actual interface name if known
    if (ifaddr->ifa_name == NULL) {
        free(ifaddr);
        return -1;
    }

    // Allocate memory for the sockaddr structure
    struct sockaddr_in *addr_in = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    if (addr_in == NULL) {
        free(ifaddr->ifa_name);
        free(ifaddr);
        return -1;
    }

    // Retrieve IP information from the ESP netif
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        free(ifaddr->ifa_name);
        free(ifaddr);
        return -1;
    }
    if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
        free(addr_in);
        free(ifaddr->ifa_name);
        free(ifaddr);
        return -1;
    }

    // Set up the sockaddr_in structure
    addr_in->sin_family = AF_INET;
    addr_in->sin_addr.s_addr = ip.ip.addr;

    // Link the sockaddr to ifaddrs
    ifaddr->ifa_addr = (struct sockaddr *)addr_in;
    ifaddr->ifa_flags = IFF_UP; // Mark the interface as UP, add more flags as needed

    *ifap = ifaddr; // Return the linked list
    return 0; // Success
}

void freeifaddrs(struct ifaddrs *ifa)
{
    while (ifa != NULL) {
        struct ifaddrs *next = ifa->ifa_next;
        if (ifa->ifa_name) {
            free(ifa->ifa_name);
        }
        if (ifa->ifa_addr) {
            free(ifa->ifa_addr);
        }
        free(ifa);
        ifa = next;
    }
}
