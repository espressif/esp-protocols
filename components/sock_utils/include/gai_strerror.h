/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lwip/sockets.h"
#include "netdb_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_IDF_TARGET_LINUX
// namespace with esp_ on linux to avoid duplication of symbols
#define gai_strerror esp_gai_strerror
#endif

/**
* @brief Returns a numeric string representing of `getaddrinfo()` error code.
*
* @param[in] ecode Error code returned by `getaddrinfo()`.
*
* @return A pointer to a string describing the error.
*/
const char *gai_strerror(int ecode);

#ifdef __cplusplus
}
#endif
