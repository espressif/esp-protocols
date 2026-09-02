/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "mdns_mem_caps.h"
#include "mdns_mem_caps_test.h"

static size_t s_allocation_counts;

size_t mdns_mem_get_allocation_counts(void)
{
    return s_allocation_counts;
}

void *mdns_mem_malloc(size_t size)
{
    void *ptr = malloc(size);
    if (ptr) {
        s_allocation_counts++;
    }
    return ptr;
}

void *mdns_mem_calloc(size_t num, size_t size)
{
    void *ptr = calloc(num, size);
    if (ptr) {
        s_allocation_counts++;
    }
    return ptr;
}

void mdns_mem_free(void *ptr)
{
    if (ptr) {
        if (s_allocation_counts > 0) {
            s_allocation_counts--;
        }
        free(ptr);
    }
}

char *mdns_mem_strdup(const char *s)
{
    if (s == NULL) {
        return NULL;
    }

    size_t len = strlen(s) + 1;
    char *new_str = mdns_mem_malloc(len);
    if (new_str == NULL) {
        return NULL;
    }

    return memcpy(new_str, s, len);
}

char *mdns_mem_strndup(const char *s, size_t n)
{
    if (s == NULL) {
        return NULL;
    }

    size_t len = strnlen(s, n);

    char *new_str = mdns_mem_malloc(len + 1);
    if (new_str == NULL) {
        return NULL;
    }

    memcpy(new_str, s, len);
    new_str[len] = '\0';

    return new_str;
}

void *mdns_mem_task_malloc(size_t size)
{
    return mdns_mem_malloc(size);
}

void mdns_mem_task_free(void *ptr)
{
    mdns_mem_free(ptr);
}
