/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "gethostname.h"
#include "esp_netif.h"
#include "errno.h"
#include "esp_log.h"


static bool highest_prio_netif(esp_netif_t *netif, void *ctx)
{
    esp_netif_t **highest_so_far = ctx;
    if (esp_netif_get_route_prio(netif) > esp_netif_get_route_prio(*highest_so_far)) {
        *highest_so_far = netif;
    }
    return false;   // go over the entire list to find the netif with the highest route-prio
}

int gethostname(char *name, size_t len)
{
    if (name == NULL) {
        errno = EINVAL;
        return -1;
    }
    const char *netif_hostname = CONFIG_LWIP_LOCAL_HOSTNAME;    // default value from Kconfig

    // Find the default netif
    esp_netif_t *default_netif = esp_netif_get_default_netif();
    if (default_netif == NULL) {    // if no netif is active/up -> find the highest prio netif
        esp_netif_find_if(highest_prio_netif, &default_netif);
    }
    // now the `default_netif` could be NULL and/or the esp_netif_get_hostname() could fail
    // but we ignore the return code, as if it fails, the `netif_hostname` still holds the default value
    esp_netif_get_hostname(default_netif, &netif_hostname);

    size_t hostname_len;
    if (netif_hostname == NULL || len < (hostname_len = strlen(netif_hostname) + 1)) { // including the NULL terminator
        errno = EINVAL;
        return -1;
    }
    memcpy(name, netif_hostname, hostname_len);
    return 0;
}
