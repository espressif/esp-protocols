/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#undef _POSIX_C_SOURCE    /* Note: mosquitto's config.h bug: defines _POSIX_C_SOURCE=200809L
                           * - fixed in v2.1.0, but IDF port doesn't support yet (due to json deps)
                           */
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

#define VERSION "v2.0.20~8"
