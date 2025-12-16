/*
 * SPDX-FileCopyrightText: 2024 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */
#include "signal.h"
#include "sdkconfig.h"

#ifndef CONFIG_IDF_TARGET_LINUX
int sigprocmask(int, const sigset_t *, sigset_t *)
{
    return 0;
}
#endif
