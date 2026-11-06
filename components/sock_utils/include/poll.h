/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

// POSIX declares the poll API in <poll.h>, ESP-IDF provides it as <sys/poll.h>:
// libraries ported from Linux include <poll.h> for struct pollfd, poll() and the POLL* flags
#include <sys/poll.h>
