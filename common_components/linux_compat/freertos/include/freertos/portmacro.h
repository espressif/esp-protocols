/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdint.h>

typedef struct {
    uint32_t owner;
    uint32_t count;
} spinlock_t;

typedef spinlock_t portMUX_TYPE;

/**< Spinlock initializer */
#define portMUX_INITIALIZER_UNLOCKED {                      \
            .owner = portMUX_FREE_VAL,                      \
            .count = 0,                                     \
        }

static inline BaseType_t xPortGetCoreID(void)
{
    return (BaseType_t) 0;
}

void vPortEnterCritical(void);
void vPortExitCritical(void);


#define portENTER_CRITICAL(mux)                 {(void)mux;  vPortEnterCritical();}
#define portEXIT_CRITICAL(mux)                  {(void)mux;  vPortExitCritical();}
#define portMUX_FREE_VAL 0
#define CONFIG_FREERTOS_NUMBER_OF_CORES 1
