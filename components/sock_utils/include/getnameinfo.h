/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lwip/sockets.h"

#ifdef CONFIG_IDF_TARGET_LINUX
// namespace with esp_ on linux to avoid conflict of symbols
#define getnameinfo esp_getnameinfo
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Converts a socket address to a corresponding host and service name.
*
* @param[in]  addr    Pointer to the socket address structure.
* @param[in]  addrlen Length of the socket address.
* @param[out] host    Buffer to store the host name.
* @param[in]  hostlen Length of the host buffer.
* @param[out] serv    Buffer to store the service name.
* @param[in]  servlen Length of the service buffer.
* @param[in]  flags   Flags to modify the behavior of the function.
*
* @return
*     - 0 on success.
*     - Non-zero on failure, with `errno` set to indicate the error.
*/
int getnameinfo(const struct sockaddr *addr, socklen_t addrlen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags);

#ifdef __cplusplus
}
#endif
