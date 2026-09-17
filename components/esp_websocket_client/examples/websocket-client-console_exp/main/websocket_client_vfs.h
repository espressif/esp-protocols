/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include "esp_err.h"
#include "esp_websocket_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* base_path;
    int send_timeout_ms;
    int recv_timeout_ms;
    size_t recv_buffer_size;
    FILE* fallback_stdout;
} websocket_client_vfs_config_t;

esp_err_t websocket_client_vfs_register(const websocket_client_vfs_config_t *config);

esp_err_t websocket_client_vfs_add_client(esp_websocket_client_handle_t handle, int id);

esp_err_t websocket_client_vfs_del_client(esp_websocket_client_handle_t handle);

esp_err_t websocket_client_vfs_event_handler(esp_websocket_client_handle_t handle, int32_t event_id, const esp_websocket_event_data_t *event_data);

#ifdef __cplusplus
}
#endif
