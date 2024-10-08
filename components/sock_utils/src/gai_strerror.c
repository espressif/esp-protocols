/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "gai_strerror.h"

_Thread_local char gai_strerror_string[32];

const char *gai_strerror(int ecode)
{
    if (snprintf(gai_strerror_string, sizeof(gai_strerror_string), "EAI error:%d", ecode) < 0) {
        return "gai_strerror() failed";
    }
    return gai_strerror_string;
}
