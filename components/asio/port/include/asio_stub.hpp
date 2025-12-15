/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <signal.h>

extern "C" int pthread_sigmask(int, const sigset_t *, sigset_t *);
