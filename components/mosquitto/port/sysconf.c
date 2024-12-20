/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <ctype.h>
#include <unistd.h>

#define _SC_OPEN_MAX_OVERRIDE                 4

// used to override syscall for obtaining max open fds
long sysconf(int arg)
{
    if (arg == _SC_OPEN_MAX_OVERRIDE) {
        return 64;
    }
    return -1;
}
