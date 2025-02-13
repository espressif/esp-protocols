/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "mdns_private.h"
#include "mdns_mem_caps.h"
#include "esp_heap_caps.h"

#ifndef MDNS_MEMORY_CAPS
#warning "No memory allocation method defined, using internal memory"
#define MDNS_MEMORY_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif

void *mdns_mem_malloc(size_t size)
{
    return heap_caps_malloc(size, MDNS_MEMORY_CAPS);
}

void *mdns_mem_calloc(size_t num, size_t size)
{
    return heap_caps_calloc(num, size, MDNS_MEMORY_CAPS);
}

void mdns_mem_free(void *ptr)
{
    heap_caps_free(ptr);
}

char *mdns_mem_strdup(const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *copy = (char *)heap_caps_malloc(len, MDNS_MEMORY_CAPS);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

char *mdns_mem_strndup(const char *s, size_t n)
{
    if (!s) {
        return NULL;
    }
    size_t len = strnlen(s, n);
    char *copy = (char *)heap_caps_malloc(len + 1, MDNS_MEMORY_CAPS);
    if (copy) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}
