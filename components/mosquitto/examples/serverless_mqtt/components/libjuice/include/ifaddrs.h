/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

// Purpose of this header is to replace udp_sendto() to avoid name conflict with lwip
// added here since ifaddrs.h is included from juice_udp sources
#define udp_sendto juice_udp_sendto

// other than that, let's just include the ifaddrs (from sock_utils)
#include_next "ifaddrs.h"
