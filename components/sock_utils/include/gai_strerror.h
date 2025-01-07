/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
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
* @brief Returns a string representing of `getaddrinfo()` error code.
*
* @param[in] errcode Error code returned by `getaddrinfo()`.
*
* @return A pointer to a string containing the error code, for example "EAI_NONAME"
* for EAI_NONAME error type.
*/
const char *gai_strerror(int errcode);

#ifdef __cplusplus
}
#endif
