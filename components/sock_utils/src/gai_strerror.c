/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "gai_strerror.h"
#include "lwip/netdb.h"

#define HANDLE_GAI_ERROR(code) \
    case code: return #code;

const char *gai_strerror(int errcode)
{
    switch (errcode) {
        /* lwip defined DNS codes */
        HANDLE_GAI_ERROR(EAI_BADFLAGS)
        HANDLE_GAI_ERROR(EAI_FAIL)
        HANDLE_GAI_ERROR(EAI_FAMILY)
        HANDLE_GAI_ERROR(EAI_MEMORY)
        HANDLE_GAI_ERROR(EAI_NONAME)
        HANDLE_GAI_ERROR(EAI_SERVICE)
        /* other error codes optionally defined in platform/newlib or toolchain */
#ifdef EAI_AGAIN
        HANDLE_GAI_ERROR(EAI_AGAIN)
#endif
#ifdef EAI_SOCKTYPE
        HANDLE_GAI_ERROR(EAI_SOCKTYPE)
#endif
#ifdef EAI_SYSTEM
        HANDLE_GAI_ERROR(EAI_SYSTEM)
#endif
    default:
        return "Unknown error";
    }
}
