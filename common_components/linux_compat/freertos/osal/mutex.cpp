/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <queue>
#include <mutex>
#include "osal_api.h"

void *osal_mutex_create()
{
    auto mut = new std::recursive_mutex();
    return mut;
}

void osal_mutex_delete(void *mut)
{
    delete static_cast<std::recursive_mutex *>(mut);
}

void osal_mutex_take(void *m)
{
    auto mut = static_cast<std::recursive_mutex *>(m);
    mut->lock();
}

void osal_mutex_give(void *m)
{
    auto mut = static_cast<std::recursive_mutex *>(m);
    mut->unlock();
}
