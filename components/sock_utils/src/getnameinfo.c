/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "ifaddrs.h"
#include "esp_check.h"
#include "esp_netif.h"

#if CONFIG_LWIP_IPV6
struct scope_name {
    u32_t index;
    char name[NETIF_NAMESIZE];
};

// The name of the interface with the index, like if_indextoname(), which lwIP provides only with CONFIG_LWIP_NETIF_API
static esp_err_t scope_name_unsafe(void *ctx)
{
    struct scope_name *scope = ctx;
    esp_netif_t *netif = NULL;
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        if ((u32_t)esp_netif_get_netif_impl_index(netif) == scope->index) {
            return esp_netif_get_netif_impl_name(netif, scope->name);
        }
    }
    return ESP_ERR_NOT_FOUND;
}
#endif

int getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags)
{
    // Validate flags to ensure only allowed flags are passed.
    if (flags & ~(NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM)) {
        return EAI_BADFLAGS;
    }

    const void *ip;         // address in network byte order
    in_port_t port;         // port in network byte order
#if CONFIG_LWIP_IPV6
    u32_t scope_id = 0;
    bool link_local = false;
#endif

    switch (addr->sa_family) {
    case AF_INET: {
        const struct sockaddr_in *sinp = (const struct sockaddr_in *) addr;
        ip = &sinp->sin_addr;
        port = sinp->sin_port;
        break;
    }
#if CONFIG_LWIP_IPV6
    case AF_INET6: {
        const struct sockaddr_in6 *sin6p = (const struct sockaddr_in6 *) addr;
        ip = &sin6p->sin6_addr;
        port = sin6p->sin6_port;
        scope_id = sin6p->sin6_scope_id;
        ip6_addr_t ip6;     // IN6_IS_ADDR_LINKLOCAL() and IN6_IS_ADDR_MC_LINKLOCAL() are not in older lwIP
        inet6_addr_to_ip6addr(&ip6, &sin6p->sin6_addr);
        link_local = ip6_addr_islinklocal(&ip6) || ip6_addr_ismulticast_linklocal(&ip6);
        break;
    }
#endif
    default:
        return EAI_FAMILY;  // Unsupported address family
    }

    // Handle numeric host address
    if (flags & NI_NUMERICHOST) {
        if (host == NULL || hostlen == 0) {
            return 0;
        }
        if (inet_ntop(addr->sa_family, ip, host, hostlen) == NULL) {
            return EOVERFLOW;  // Error if address couldn't be converted
        }
#if CONFIG_LWIP_IPV6
        // Append the scope like glibc does: the interface name for a link-local address (e.g. fe80::1%en1),
        // else, or if there is no such interface, the index (e.g. 2001:db8::1%2). See gni_host_inet_numeric() in
        // https://sourceware.org/git/?p=glibc.git;a=blob;f=nss/getnameinfo.c;hb=glibc-2.44#l344
        if (scope_id != 0) {
            const size_t len = strlen(host);
            struct scope_name scope = { .index = scope_id };
            const int written = link_local && esp_netif_tcpip_exec(scope_name_unsafe, &scope) == ESP_OK
                                ? snprintf(host + len, hostlen - len, "%%%s", scope.name)
                                : snprintf(host + len, hostlen - len, "%%%u", (unsigned)scope_id);
            if (written < 0 || (size_t)written >= hostlen - len) {
                return EOVERFLOW;  // Error if the scope did not fit
            }
        }
#endif
    }

    // Handle numeric service (port)
    if (flags & NI_NUMERICSERV) {
        // Print the port number, but for UDP services (if NI_DGRAM), we treat it the same way
        if (serv == NULL || servlen == 0) {
            return 0;
        }
        if (snprintf(serv, servlen, "%d", ntohs(port)) < 0) {
            return EOVERFLOW;  // Error if snprintf failed
        }
    }

    return 0;
}
