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
// namespace with esp_ on linux to avoid conflict of symbols
#define socketpair esp_socketpair
#define pipe esp_pipe
#endif

/**
* @brief Creates a pair of connected sockets.
*
* @param[in]  domain   Communication domain (e.g., AF_UNIX).
* @param[in]  type     Socket type (e.g., SOCK_STREAM).
* @param[in]  protocol Protocol to be used (usually 0).
* @param[out] sv       Array of two integers to store the file descriptors of the created sockets.
*
* @return
*     - 0 on success.
*     - -1 on failure, with `errno` set to indicate the error.
*/
int socketpair(int domain, int type, int protocol, int sv[2]);

/**
 * @brief Creates a unidirectional data channel (pipe).
 *
 * @param[out] pipefd Array of two integers where the file descriptors for the read and write ends
 *                    of the pipe will be stored.
 *
 * @return
 *     - 0 on success.
 *     - -1 on failure, with `errno` set to indicate the error.
 */
int pipe(int pipefd[2]);

#ifdef __cplusplus
}
#endif
