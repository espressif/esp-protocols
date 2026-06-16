/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#if __has_include("esp_assert.h")
#include_next "esp_assert.h"
#else
#define ESP_STATIC_ASSERT _Static_assert
#endif
