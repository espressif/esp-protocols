/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdbool.h>
#include <stdlib.h>
#include "esp_event_mock.h"

typedef struct esp_event_handler_node_s {
    esp_event_base_t base;
    int32_t id;
    esp_event_handler_t handler;
    void *arg;
    struct esp_event_handler_node_s *next;
} esp_event_handler_node_t;

static esp_event_handler_node_t *s_handlers;

static bool event_matches_handler(const esp_event_handler_node_t *handler,
                                  esp_event_base_t event_base, int32_t event_id)
{
    return (handler->base == ESP_EVENT_ANY_BASE || handler->base == event_base) &&
           (handler->id == ESP_EVENT_ANY_ID || handler->id == event_id);
}

static void clear_event_handlers(void)
{
    while (s_handlers) {
        esp_event_handler_node_t *handler = s_handlers;
        s_handlers = handler->next;
        free(handler);
    }
}

esp_err_t esp_event_post(esp_event_base_t event_base, int32_t event_id,
                         const void *event_data, size_t event_data_size,
                         TickType_t ticks_to_wait)
{
    (void)ticks_to_wait;

    for (esp_event_handler_node_t *handler = s_handlers; handler; handler = handler->next) {
        if (event_matches_handler(handler, event_base, event_id)) {
            handler->handler(handler->arg, event_base, event_id, (void *)event_data);
        }
    }
    return ESP_OK;
}

esp_err_t esp_event_handler_register(esp_event_base_t event_base, int32_t event_id,
                                     esp_event_handler_t event_handler, void *event_handler_arg)
{
    if (!event_handler) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_event_handler_node_t *handler = calloc(1, sizeof(*handler));
    if (!handler) {
        return ESP_ERR_NO_MEM;
    }

    handler->base = event_base;
    handler->id = event_id;
    handler->handler = event_handler;
    handler->arg = event_handler_arg;

    esp_event_handler_node_t **last_handler = &s_handlers;
    while (*last_handler) {
        last_handler = &(*last_handler)->next;
    }
    *last_handler = handler;
    return ESP_OK;
}

esp_err_t esp_event_handler_unregister(esp_event_base_t event_base, int32_t event_id,
                                       esp_event_handler_t event_handler)
{
    esp_event_handler_node_t **handler = &s_handlers;
    while (*handler) {
        if ((*handler)->base == event_base && (*handler)->id == event_id && (*handler)->handler == event_handler) {
            esp_event_handler_node_t *removed_handler = *handler;
            *handler = removed_handler->next;
            free(removed_handler);
            return ESP_OK;
        }
        handler = &(*handler)->next;
    }

    return ESP_ERR_INVALID_ARG;
}

void mdns_test_esp_event_reset(void)
{
    clear_event_handlers();
}
