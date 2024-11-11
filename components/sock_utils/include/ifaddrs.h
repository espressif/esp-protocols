/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lwip/sockets.h"
#include "netdb_macros.h"
// include also other related headers to simplify porting of linux libs
#include "getnameinfo.h"
#include "socketpair.h"
#include "gai_strerror.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_IDF_TARGET_LINUX
// namespace with esp_ on linux to avoid duplication of symbols
#define getifaddrs esp_getifaddrs
#define freeifaddrs esp_freeifaddrs
#endif

/**
 * @brief Simplified version of ifaddr struct
 */
struct ifaddrs {
    struct ifaddrs  *ifa_next;    /* Next item in list */
    char            *ifa_name;    /* Name of interface */
    struct sockaddr *ifa_addr;    /* Address of interface */
    unsigned int ifa_flags;
};

/**
 * @brief Retrieves a list of network interfaces and their addresses.
 *
 * @param[out] ifap Pointer to a linked list of `struct ifaddrs`. On success, `*ifap` will be set
 *                  to the head of the list.
 *
 * @return
 *     - 0 on success.
 *     - -1 on failure, with `errno` set to indicate the error.
 */
int getifaddrs(struct ifaddrs **ifap);

/**
 * @brief Frees the memory allocated by `getifaddrs()`.
 *
 * @param[in] ifa Pointer to the linked list of network interfaces to be freed.
 */
void freeifaddrs(struct ifaddrs *ifa);

#ifdef __cplusplus
}
#endif
