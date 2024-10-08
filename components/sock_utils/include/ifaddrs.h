/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST  0x1
#endif

#ifndef IFF_UP
#define IFF_UP 0x1
#endif

#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif

struct ifaddrs {
    struct ifaddrs  *ifa_next;    /* Next item in list */
    char            *ifa_name;    /* Name of interface */
    struct sockaddr *ifa_addr;    /* Address of interface */
    int ifa_flags;
};

int getifaddrs(struct ifaddrs **ifap);

void freeifaddrs(struct ifaddrs *ifa);
