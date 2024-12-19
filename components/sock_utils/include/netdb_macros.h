/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

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
