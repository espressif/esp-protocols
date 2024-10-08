/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <fcntl.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "ifaddrs.h"
#include "esp_check.h"

int getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags)
{
    // Validate flags to ensure only allowed flags are passed.
    if (flags & ~(NI_NUMERICHOST | NI_NUMERICSERV | NI_DGRAM)) {
        return EAI_BADFLAGS;
    }

    const struct sockaddr_in *sinp = (const struct sockaddr_in *) addr;

    switch (addr->sa_family) {
    case AF_INET:
        // Handle numeric host address
        if (flags & NI_NUMERICHOST) {
            if (host == NULL || hostlen == 0) {
                return 0;
            }
            if (inet_ntop(AF_INET, &sinp->sin_addr, host, hostlen) == NULL) {
                return EOVERFLOW;  // Error if address couldn't be converted
            }
        }

        // Handle numeric service (port)
        if (flags & NI_NUMERICSERV) {
            // Print the port number, but for UDP services (if NI_DGRAM), we treat it the same way
            int port = ntohs(sinp->sin_port);  // Convert port from network byte order
            if (serv == NULL || servlen == 0) {
                return 0;
            }
            if (snprintf(serv, servlen, "%d", port) < 0) {
                return EOVERFLOW;  // Error if snprintf failed
            }
        }

        break;

    default:
        return EAI_FAMILY;  // Unsupported address family
    }

    return 0;
}
