/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <ctype.h>
#include "net/if.h"

#undef  isspace
#define isspace(__c) (__ctype_lookup((int)__c)&_S)

#include_next "config.h"

#define VERSION "v2.0.20~2"
