/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/* UTC timegm() for the libwebsockets build on ESP-IDF.
 *
 * ESP's picolibc ships timegm() in libc but its <time.h> prototype is gated
 * behind BSD/GNU visibility, and the symbol is not reliably linkable in the
 * IDF cross-build, so lws's CHECK_FUNCTION_EXISTS(timegm) fails. With
 * LWS_HAVE_TIMEGM off, lws_http_date_parse_unix() falls back to mktime(),
 * which interprets a struct tm in the LOCAL timezone — so a server "GMT" Date
 * header is misread by the configured TZ offset, corrupting HTTP clock-skew
 * handling (e.g. AWS SigV4 signing). Provide a dependency-free UTC conversion.
 */
#include <time.h>

static int lws_is_leap(int y)
{
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

/* weak: if a future picolibc/newlib exports a linkable timegm(), the libc one
 * wins and this definition is discarded — no multiple-definition error. */
__attribute__((weak)) time_t timegm(struct tm *tm)
{
    static const int mdays_cum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    int year = tm->tm_year + 1900;
    int month = tm->tm_mon;
    /* Normalize month into [0,11], carrying the remainder into the year.
     * Works for negative tm_mon too (including exact multiples of -12), so
     * mdays_cum[] is never indexed out of bounds. */
    year += month / 12;
    month %= 12;
    if (month < 0) {
        month += 12;
        year -= 1;
    }

    long long days = 0;
    if (year >= 1970) {
        for (int y = 1970; y < year; ++y) {
            days += 365 + lws_is_leap(y);
        }
    } else {
        for (int y = year; y < 1970; ++y) {
            days -= 365 + lws_is_leap(y);
        }
    }

    days += mdays_cum[month];
    if (month > 1 && lws_is_leap(year)) {
        days += 1;
    }
    days += (tm->tm_mday - 1);

    return (time_t)(days * 86400LL
                    + (long long)tm->tm_hour * 3600
                    + (long long)tm->tm_min * 60
                    + (long long)tm->tm_sec);
}
