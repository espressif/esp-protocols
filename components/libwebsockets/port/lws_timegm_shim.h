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

/* Only the ESP libc (picolibc/newlib) hides timegm()'s prototype behind BSD/GNU
 * visibility; declare it for those. A libc that already declares it (e.g. glibc)
 * needs no redeclaration. `#pragma once` already guards double-inclusion. */
#if defined(__PICOLIBC__) || defined(__NEWLIB__)
time_t timegm(struct tm *__tp);
#endif
