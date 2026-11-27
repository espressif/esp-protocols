/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "mdns_private.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Select how the networking stub reports interface readiness
 *
 * With tracking off (the default), every interface is ready. With tracking on, an interface is
 * ready only between mdns_priv_if_init() and mdns_priv_if_deinit(). Each call resets the readiness
 * and the init counts.
 */
void mdns_test_networking_track_ready(bool track);

/**
 * @brief  Number of mdns_priv_if_init() calls for an interface since the last
 *         mdns_test_networking_track_ready()
 */
size_t mdns_test_networking_init_count(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);

#ifdef __cplusplus
}
#endif
