/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include_next "config.h"

/*
 * =======================================================================
 *  Do NOT include any other header before this comment.
 *  config.h header configures critical macros that must be set before any
 *  system or standard library headers are included.
 * =======================================================================
 */

#include <ctype.h>
#include "net/if.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION_MAJOR < 6
#undef  isspace
#define isspace(__c) (__ctype_lookup((int)__c)&_S)
#endif

#define VERSION "v2.0.20~5"
