/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <time.h>
#include "time_mosq.h"

#include "esp_timer.h"

void mosquitto_time_init(void)
{
}

time_t mosquitto_time(void)
{
    return esp_timer_get_time() / 1000000; // Convert microseconds to seconds
}
