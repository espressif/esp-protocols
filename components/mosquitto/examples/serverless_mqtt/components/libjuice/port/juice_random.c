/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "esp_random.h"

void juice_random(void *buf, size_t size)
{
    esp_fill_random(buf, size);
}

void juice_random_str64(char *buf, size_t size)
{
    static const char chars64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0;
    for (i = 0; i + 1 < size; ++i) {
        uint8_t byte = 0;
        juice_random(&byte, 1);
        buf[i] = chars64[byte & 0x3F];
    }
    buf[i] = '\0';
}

uint32_t juice_rand32(void)
{
    uint32_t r = 0;
    juice_random(&r, sizeof(r));
    return r;
}

uint64_t juice_rand64(void)
{
    uint64_t r = 0;
    juice_random(&r, sizeof(r));
    return r;
}
