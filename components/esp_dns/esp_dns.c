/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * esp_dns.c - Custom DNS module for ESP32 with multiple protocol support
 *
 * Provides DNS resolution capabilities with support for various protocols:
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
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
//#include "esp_netif.h"
#include "esp_system.h"
#include "lwip/api.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "esp_dns.h"
#include "esp_dns_priv.h"
#include "esp_dns_udp.h"
#include "esp_dns_tcp.h"
#include "esp_dns_dot.h"
#include "esp_dns_doh.h"

#define TAG "ESP_DNS"


/* Global DNS handle instance */
static esp_dns_handle_t g_dns_handle = NULL;

/* Mutex for protecting global handle access */
static SemaphoreHandle_t g_dns_global_mutex = NULL;


esp_dns_handle_t dns_create_handle(void)
{
    static struct dns_handle instance;
    static bool initialized = false;

    if (!initialized) {
        memset(&instance, 0, sizeof(instance));
        initialized = true;
    }

    return &instance;
}

/**
 * Initialize the DNS module with provided configuration.
 *
 * @param config Pointer to esp_dns_config_t structure with desired DNS configuration
 * @return On success, returns a handle to the initialized DNS module.
 *         On failure, returns NULL.
 */
esp_dns_handle_t esp_dns_init(const esp_dns_config_t *config)
{
    /* Validate parameters */
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration (NULL)");
        return NULL;
    }

    /* Create global mutex if it doesn't exist */
    if (g_dns_global_mutex == NULL) {
        g_dns_global_mutex = xSemaphoreCreateMutex();
        if (g_dns_global_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create global mutex");
            return NULL;
        }
    }

    /* Take the global mutex */
    if (xSemaphoreTake(g_dns_global_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take global mutex");
        return NULL;
    }

    /* Check if we need to clean up an existing handle */
    if (g_dns_handle != NULL) {
        ESP_LOGI(TAG, "Replacing existing DNS configuration");
        esp_dns_cleanup(g_dns_handle);
        g_dns_handle = NULL;
    }

    /* Allocate memory for the new handle */
    esp_dns_handle_t handle = dns_create_handle();
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for DNS handle");
        xSemaphoreGive(g_dns_global_mutex);
        return NULL;
    }

    /* Copy configuration */
    memcpy(&handle->config, config, sizeof(esp_dns_config_t));

    /* Create mutex for this handle */
    handle->lock = xSemaphoreCreateMutex();
    if (handle->lock == NULL) {
        ESP_LOGE(TAG, "Failed to create handle mutex");
        free(handle);
        xSemaphoreGive(g_dns_global_mutex);
        return NULL;
    }

    /* Initialize protocol-specific state based on selected protocol */
    int ret = -1;
    switch (config->protocol) {
    case DNS_PROTOCOL_UDP:
        ret = init_udp_dns(handle);
        break;

    case DNS_PROTOCOL_TCP:
        ret = init_tcp_dns(handle);
        break;

    case DNS_PROTOCOL_DOT:
        ret = init_dot_dns(handle);
        break;

    case DNS_PROTOCOL_DOH:
        ret = init_doh_dns(handle);
        break;

    default:
        ESP_LOGE(TAG, "Unsupported DNS protocol: %d", config->protocol);
        ret = -1;
        break;
    }

    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to initialize DNS protocol %d: Error Code: %d",
                 config->protocol, handle->last_error);

        /* Clean up resources */
        if (handle->lock != NULL) {
            vSemaphoreDelete(handle->lock);
        }
        free(handle);
        xSemaphoreGive(g_dns_global_mutex);
        return NULL;
    }

    /* Set global handle */
    g_dns_handle = handle;
    handle->initialized = true;

    /* Release global mutex */
    xSemaphoreGive(g_dns_global_mutex);

    ESP_LOGI(TAG, "DNS module initialized successfully with protocol %d", config->protocol);
    return handle;
}

/**
 * Cleanup and release resources associated with a DNS module handle.
 *
 * @param handle DNS module handle previously obtained from esp_dns_init()
 * @return 0 on success, non-zero error code on failure
 */
int esp_dns_cleanup(esp_dns_handle_t handle)
{
    /* Validate parameters */
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle (NULL)");
        return -1;
    }

    /* Take the handle mutex */
    if (xSemaphoreTake(handle->lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take handle mutex during cleanup");
        return -1;
    }

    /* Clean up protocol-specific resources */
    int ret = 0;
    switch (handle->config.protocol) {
    case DNS_PROTOCOL_UDP:
        ret = cleanup_udp_dns(handle);
        break;

    case DNS_PROTOCOL_TCP:
        ret = cleanup_tcp_dns(handle);
        break;

    case DNS_PROTOCOL_DOT:
        ret = cleanup_dot_dns(handle);
        break;

    case DNS_PROTOCOL_DOH:
        ret = cleanup_doh_dns(handle);
        break;

    default:
        ESP_LOGW(TAG, "Unknown protocol during cleanup: %d", handle->config.protocol);
        ret = -1;
        break;
    }

    /* Mark as uninitialized */
    handle->initialized = false;

    /* Release and delete mutex */
    xSemaphoreGive(handle->lock);
    vSemaphoreDelete(handle->lock);

    /* Take global mutex before modifying global handle */
    if (g_dns_global_mutex != NULL && xSemaphoreTake(g_dns_global_mutex, portMAX_DELAY) == pdTRUE) {
        /* Clear global handle if it matches this one */
        if (g_dns_handle == handle) {
            g_dns_handle = NULL;
        }

        xSemaphoreGive(g_dns_global_mutex);
    }

    /* Empty the handle */
    memset(handle, 0, sizeof(handle));

    ESP_LOGI(TAG, "DNS module cleaned up %s", (ret == 0) ? "successfully" : "with errors");
    return ret;
}

/**
 * Get the last error code from the DNS module.
 *
 * @return Last error code
 */
int dns_get_last_error(esp_dns_handle_t handle)
{
    if (handle == NULL) {
        return DNS_ERR_INIT_FAILED;
    }

    return handle->last_error;
}


/* ========================= LWIP HOOK FUNCTIONS ========================= */

#if defined(CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM)
int lwip_hook_netconn_external_resolve(const char *name, ip_addr_t *addr, u8_t addrtype, err_t *err)
{
    if (g_dns_handle == NULL) {
        ESP_LOGD(TAG, "ESP_DNS module not initialized, resolving through native DNS");
        *err = ERR_OK;
        return 0;
    }

    if (name == NULL || addr == NULL || err == NULL) {
        if (err) {
            *err = ERR_ARG;
        }
        return 1;
    }

    /* Check if name is already an IP address */
    if (ipaddr_aton(name, addr)) {
        *err = ERR_OK;
        return 0;
    }

    /* Check if HTTPS_DNS_SERVER is in the dns cache */
    if ((strcmp(name, g_dns_handle->config.dns_server) == 0) ||
#if LWIP_HAVE_LOOPIF
            /* Check if it's localhost */
            (strcmp(name, "localhost") == 0) ||
#endif
            ipaddr_aton(name, addr)) { /* host name already in octet notation */
        return 0;
    }

    u8_t rrtype;
    if ((addrtype == NETCONN_DNS_IPV4) || (addrtype == NETCONN_DNS_IPV4_IPV6)) {
        rrtype = DNS_RRTYPE_A;
    } else if ((addrtype == NETCONN_DNS_IPV6) || (addrtype == NETCONN_DNS_IPV6_IPV4)) {
        rrtype = DNS_RRTYPE_AAAA;
    } else {
        ESP_LOGE(TAG, "Invalid address type");
        *err = ERR_VAL;
        return 1;
    }

    /* Resolve based on configured transport type */
    switch (g_dns_handle->config.protocol) {
    case DNS_PROTOCOL_UDP:
        /* Return zero as lwip dns can handle dns udp */
        return 0;
    case DNS_PROTOCOL_TCP:
        *err = dns_resolve_tcp(g_dns_handle, name, addr, rrtype);
        break;
    case DNS_PROTOCOL_DOT:
        *err = dns_resolve_dot(g_dns_handle, name, addr, rrtype);
        break;
    case DNS_PROTOCOL_DOH:
        *err = dns_resolve_doh(g_dns_handle, name, addr, rrtype);
        break;
    default:
        ESP_LOGE(TAG, "Invalid transport type");
        *err = ERR_VAL;
    }

    return 1;
}
#endif /* CONFIG_LWIP_HOOK_NETCONN_EXT_RESOLVE_CUSTOM */
