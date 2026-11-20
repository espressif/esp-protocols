/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

void mdns_test_tx_capture_clear(void);
const uint8_t *mdns_test_tx_capture_data(void);
size_t mdns_test_tx_capture_len(void);
size_t mdns_test_tx_capture_count(void);
