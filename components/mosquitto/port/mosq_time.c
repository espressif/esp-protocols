/*
 * SPDX-FileCopyrightText: 2024 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * SPDX-FileContributor: 2025 Espressif Systems (Shanghai) CO LTD
 */

#include <time.h>
#include "time_mosq.h"
#include "esp_err.h"
#include "esp_timer.h"

void mosquitto_time_init(void)
{
}

time_t mosquitto_time(void)
{
    return esp_timer_get_time() / 1000000; // Convert microseconds to seconds
}
