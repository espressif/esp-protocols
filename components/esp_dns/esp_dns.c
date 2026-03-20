/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_dns.c
 * @brief Custom DNS module for ESP32 with multiple protocol support
 *
 * This module provides DNS resolution capabilities with support for various protocols:
 * - Standard UDP/TCP DNS (Port 53)
 * - DNS over TLS (DoT) (Port 853)
 * - DNS over HTTPS (DoH) (Port 443)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "esp_dns_priv.h"
#include "esp_dns.h"

#define TAG "ESP_DNS"

/* Global DNS handle instance */
esp_dns_handle_t g_dns_handle = NULL;

/* Mutex for protecting global handle access */
static SemaphoreHandle_t s_dns_global_mutex = NULL;

/**
 * @brief Creates or returns a singleton DNS handle instance
 *
 * This function implements a singleton pattern for the DNS handle. It creates
 * a static instance of the dns_handle structure on first call and initializes
 * it to zeros. On subsequent calls, it returns a pointer to the same instance.
 *
 * The function ensures that only one DNS handle exists throughout the application
 * lifecycle, which helps manage resources efficiently.
 *
 * @return Pointer to the singleton DNS handle instance
 */
static esp_dns_handle_t esp_dns_create_handle(void)
{
    static struct esp_dns_handle instance;
    static bool initialized = false;

    if (!initialized) {
        memset(&instance, 0, sizeof(instance));
        initialized = true;
    }

    return &instance;
}

/**
 * @brief Initialize the DNS module with provided configuration
 *
 * @param config DNS configuration parameters
 *
 * @return On success, returns a handle to the initialized DNS module
 *         On failure, returns NULL
 */
esp_dns_handle_t esp_dns_init(const esp_dns_config_t *config)
{
    /* Create global mutex if it doesn't exist */
    if (s_dns_global_mutex == NULL) {
        s_dns_global_mutex = xSemaphoreCreateMutex();
        if (s_dns_global_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create global mutex");
            return NULL;
        }
    }

    /* Take the global mutex */
    if (xSemaphoreTake(s_dns_global_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take global mutex");
        return NULL;
    }

    /* Check if we need to clean up an existing handle */
    if (g_dns_handle != NULL) {
        ESP_LOGE(TAG, "DNS handle already initialized. Call esp_dns_cleanup() before reinitializing");
        xSemaphoreGive(s_dns_global_mutex);
        return NULL;
    }

    /* Allocate memory for the new handle */
    esp_dns_handle_t handle = esp_dns_create_handle();
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for DNS handle");
        xSemaphoreGive(s_dns_global_mutex);
        return NULL;
    }

    /* Copy configuration */
    memcpy(&handle->config, config, sizeof(esp_dns_config_t));

    /* Create mutex for this handle */
    handle->lock = xSemaphoreCreateMutex();
    if (handle->lock == NULL) {
        ESP_LOGE(TAG, "Failed to create handle mutex");
        free(handle);
        xSemaphoreGive(s_dns_global_mutex);
        return NULL;
    }

    /* Set global handle */
    g_dns_handle = handle;
    handle->initialized = true;

    /* Release global mutex */
    xSemaphoreGive(s_dns_global_mutex);

    return handle;
}

/**
 * @brief Cleanup and release resources associated with a DNS module handle
 *
 * @param handle DNS module handle previously obtained from esp_dns_init()
 *
 * @return 0 on success, non-zero error code on failure
 */
int esp_dns_cleanup(esp_dns_handle_t handle)
{
    /* Take the handle mutex */
    if (xSemaphoreTake(handle->lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take handle mutex during cleanup");
        return -1;
    }

    /* Release and delete mutex */
    xSemaphoreGive(handle->lock);
    vSemaphoreDelete(handle->lock);

    /* Take global mutex before modifying global handle */
    if (s_dns_global_mutex != NULL && xSemaphoreTake(s_dns_global_mutex, portMAX_DELAY) == pdTRUE) {
        /* Clear global handle if it matches this one */
        if (g_dns_handle == handle) {
            g_dns_handle = NULL;
        }

        xSemaphoreGive(s_dns_global_mutex);
    }

    /* Mark as uninitialized */
    handle->initialized = false;

    return 0;
}
