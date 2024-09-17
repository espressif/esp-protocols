/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once


#define EAI_SYSTEM      11  /* system error returned in errno */

#include "esp_log.h"
#define LOG_EMERG  (0)
#define LOG_ALERT  (1)
#define LOG_CRIT   (2)
#define LOG_ERR    (3)
#define LOG_WARNING    (4)
#define LOG_NOTICE (5)
#define LOG_INFO       (6)
#define LOG_DEBUG      (7)
#define LOG_DAEMON      (8)

#define LOG_LOCAL0 (0)
#define LOG_LOCAL1 (1)
#define LOG_LOCAL2 (2)
#define LOG_LOCAL3 (3)
#define LOG_LOCAL4 (4)
#define LOG_LOCAL5 (5)
#define LOG_LOCAL6 (6)
#define LOG_LOCAL7 (7)

#define syslog(sev, format, ... ) ESP_LOG_LEVEL_LOCAL(sev-2,   "mosquitto", format, ##__VA_ARGS__)

#define gai_strerror(x) "gai_strerror() not supported"
#define openlog(a, b, c)
#define closelog()
