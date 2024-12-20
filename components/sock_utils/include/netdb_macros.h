/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#ifndef CONFIG_IDF_TARGET_LINUX
#include <netinet/in.h>   // For network-related definitions
#include <sys/socket.h>   // For socket-related definitions
#include <net/if.h>       // For interface flags (e.g., IFF_UP)
#include <netdb.h>        // For NI_NUMERICHOST, NI_NUMERICSERV, etc.
#include <errno.h>        // For EAI_BADFLAGS
#include <sys/un.h>       // For AF_UNIX
#include <sys/types.h>    // For PF_LOCAL
#endif

#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST  0x1
#endif

#ifndef IFF_UP
#define IFF_UP          0x1
#endif

#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK    0x8
#endif

#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV  0x8
#endif

#ifndef NI_DGRAM
#define NI_DGRAM        0x00000010
#endif

#ifndef EAI_BADFLAGS
#define EAI_BADFLAGS    3
#endif

#ifndef AF_UNIX
#define AF_UNIX         1
#endif

#ifndef PF_LOCAL
/*
 * In POSIX, AF_UNIX and PF_LOCAL are essentially synonymous.
 */
#define PF_LOCAL        AF_UNIX
#endif
