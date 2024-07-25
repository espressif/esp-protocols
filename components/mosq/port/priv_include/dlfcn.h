/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#define dlopen(a, b) NULL
#define dlclose(A)
#define dlsym(HANDLE, SYM) NULL
#define dlerror() "dlerror not supported"
