/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "gai_strerror.h"

const char *gai_strerror(int ecode)
{
    static char str[32];
    if (snprintf(str, sizeof(str), "EAI error:%d", ecode) < 0) {
        return "gai_strerror() failed";
    }
    return str;
}
