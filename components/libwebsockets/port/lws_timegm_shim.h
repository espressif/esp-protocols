/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/* Prototype shim, force-included into the libwebsockets build.
 *
 * picolibc gates the timegm() prototype behind BSD/GNU visibility, so lws's
 * date.c would otherwise see an implicit (int-returning) declaration and
 * truncate the 64-bit time_t. This declares it so date.c links against the
 * UTC timegm() provided in port/lws_timegm.c. See CMakeLists.txt.
 */
#pragma once
#include <time.h>

#ifndef __ESP_LWS_TIMEGM_DECLARED
#define __ESP_LWS_TIMEGM_DECLARED
time_t timegm(struct tm *__tp);
#endif
