/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void mdns_test_clock_reset(void);

bool mdns_test_clock_advance_us(int64_t delta_us);

#ifdef __cplusplus
}
#endif
