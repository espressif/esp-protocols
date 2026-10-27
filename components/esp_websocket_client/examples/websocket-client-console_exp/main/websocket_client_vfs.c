/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/ringbuf.h"
#include "esp_websocket_client.h"
#include "websocket_client_vfs.h"

#define MAX_CLIENTS 4
static const char* TAG = "websocket_client_vfs";

static ssize_t websocket_client_vfs_write(void* ctx, int fd, const void * data, size_t size);
static ssize_t websocket_client_vfs_read(void* ctx, int fd, void * dst, size_t size);
static int websocket_client_vfs_open(void* ctx, const char * path, int flags, int mode);
static int websocket_client_vfs_close(void* ctx, int fd);
static int websocket_client_vfs_fstat(void* ctx, int fd, struct stat * st);

typedef struct {
    esp_websocket_client_handle_t ws_client_handle;
    bool opened;
    RingbufHandle_t from_websocket;
} websocket_client_vfs_desc_t;

static websocket_client_vfs_desc_t s_desc[MAX_CLIENTS];
static _lock_t s_lock;
static websocket_client_vfs_config_t s_config;

esp_err_t websocket_client_vfs_register(const websocket_client_vfs_config_t *config)
{
    s_config = *config;
    const esp_vfs_t vfs = {
        .flags = ESP_VFS_FLAG_CONTEXT_PTR,
        .open_p = websocket_client_vfs_open,
        .close_p = websocket_client_vfs_close,
        .read_p = websocket_client_vfs_read,
        .write_p = websocket_client_vfs_write,
        .fstat_p = websocket_client_vfs_fstat,
    };
    return esp_vfs_register(config->base_path, &vfs, NULL);
}

esp_err_t websocket_client_vfs_event_handler(esp_websocket_client_handle_t handle, int32_t event_id, const esp_websocket_event_data_t *event_data)
{
    int fd;
    for (fd = 0; fd < MAX_CLIENTS; ++fd) {
        if (s_desc[fd].ws_client_handle == handle) {
            break;
        }
    }
    if (fd == MAX_CLIENTS) {
        // didn't find the handle
        return ESP_ERR_INVALID_ARG;
    }
    RingbufHandle_t rb = s_desc[fd].from_websocket;

    switch (event_id) {
    case WEBSOCKET_EVENT_DATA:
        if (event_data->op_code == 2 /* binary frame */) {
            xRingbufferSend(rb, event_data->data_ptr, event_data->data_len, s_config.recv_timeout_ms);
        }
        break;
    }
    return ESP_OK;
}

static ssize_t websocket_client_vfs_write(void* ctx, int fd, const void * data, size_t size)
{
    static int writing = 0;
    if (fd < 0 || fd > MAX_CLIENTS) {
        errno = EBADF;
        return -1;
    }
    if (writing) {
        // avoid re-entry, print to original stdout
        return fwrite(data, 1, size, s_config.fallback_stdout);
    }
    writing = 1;
    int sent = esp_websocket_client_send_bin(s_desc[fd].ws_client_handle, data, size, pdMS_TO_TICKS(s_config.send_timeout_ms));
    writing = 0;
    return sent;
}

static ssize_t websocket_client_vfs_read(void* ctx, int fd, void * dst, size_t size)
{
    size_t read_remaining = size;
    uint8_t* p_dst = (uint8_t*) dst;
    RingbufHandle_t rb = s_desc[fd].from_websocket;
    while (read_remaining > 0) {
        size_t read_size;
        void * ptr = xRingbufferReceiveUpTo(rb, &read_size, portMAX_DELAY, read_remaining);
        if (ptr == NULL) {
            // timeout
            errno = EIO;
            break;
        }
        memcpy(p_dst, ptr, read_size);
        vRingbufferReturnItem(rb, ptr);
        read_remaining -= read_size;
    }
    return size - read_remaining;
}

static int websocket_client_vfs_open(void* ctx, const char * path, int flags, int mode)
{
    if (path[0] != '/') {
        errno = ENOENT;
        return -1;
    }
    int fd = strtol(path + 1, NULL, 10);
    if (fd < 0 || fd >= MAX_CLIENTS) {
        errno = ENOENT;
        return -1;
    }
    int res = -1;
    _lock_acquire(&s_lock);
    if (s_desc[fd].opened) {
        errno = EPERM;
    } else {
        s_desc[fd].opened = true;
        res = fd;
    }
    _lock_release(&s_lock);
    return res;
}

static int websocket_client_vfs_close(void* ctx, int fd)
{
    if (fd < 0 || fd >= MAX_CLIENTS) {
        errno = EBADF;
        return -1;
    }
    int res = -1;
    _lock_acquire(&s_lock);
    if (!s_desc[fd].opened) {
        errno = EBADF;
    } else {
        s_desc[fd].opened = false;
        res = 0;
    }
    _lock_release(&s_lock);
    return res;
}

static int websocket_client_vfs_fstat(void* ctx, int fd, struct stat * st)
{
    *st = (struct stat) {
        0
    };
    st->st_mode = S_IFCHR;
    return 0;
}

esp_err_t websocket_client_vfs_add_client(esp_websocket_client_handle_t handle, int id)
{
    esp_err_t res = ESP_OK;
    _lock_acquire(&s_lock);
    if (s_desc[id].ws_client_handle != NULL) {
        ESP_LOGE(TAG, "%s: id=%d already in use", __func__, id);
        res = ESP_ERR_INVALID_STATE;
    } else {
        ESP_LOGD(TAG, "%s: id=%d is now in use for websocket client handle=%p", __func__, id, handle);
        s_desc[id].ws_client_handle = handle;
        s_desc[id].opened = false;
        s_desc[id].from_websocket = xRingbufferCreate(s_config.recv_buffer_size, RINGBUF_TYPE_BYTEBUF);
    }
    _lock_release(&s_lock);
    return res;
}

esp_err_t websocket_client_vfs_del_client(esp_websocket_client_handle_t handle)
{
    esp_err_t res = ESP_ERR_INVALID_ARG;
    _lock_acquire(&s_lock);
    for (int id = 0; id < MAX_CLIENTS; ++id) {
        if (s_desc[id].ws_client_handle != handle) {
            continue;
        }
        if (s_desc[id].ws_client_handle != NULL) {
            ESP_LOGE(TAG, "%s: id=%d already in use", __func__, id);
            res = ESP_ERR_INVALID_STATE;
            break;
        } else {
            ESP_LOGD(TAG, "%s: id=%d is now in use for websocket client handle=%p", __func__, id, handle);
            s_desc[id].ws_client_handle = NULL;
            s_desc[id].opened = false;
            vRingbufferDelete(s_desc[id].from_websocket);
            res = ESP_OK;
            break;
        }
    }
    _lock_release(&s_lock);
    return res;
}
