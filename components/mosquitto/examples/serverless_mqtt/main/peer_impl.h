/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "esp_random.h"
#include "esp_sleep.h"
#include "mosq_broker.h"

typedef void (*on_peer_recv_t)(const char *data, size_t size);

esp_err_t peer_init(on_peer_recv_t cb);

void peer_get_buffer(char ** buffer, size_t *buffer_len);

void peer_send(char* data, size_t size);
