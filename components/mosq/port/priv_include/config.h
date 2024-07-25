/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <ctype.h>

#undef  isspace
#define isspace(__c) (__ctype_lookup((int)__c)&_S)

#include_next "config.h"
#define VERSION "v2.0.18~0"

#define _SC_OPEN_MAX_OVERRIDE                 4

// used to override syscall for obtaining max open fds
static inline long sysconf(int arg)
{
    if (arg == _SC_OPEN_MAX_OVERRIDE) {
        return 64;
    }
    return -1;
}
