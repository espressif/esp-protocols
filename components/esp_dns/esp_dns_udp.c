/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_dns_priv.h"
#include "esp_dns.h"


#define TAG "ESP_DNS_UDP"


/**
 * @brief Initializes the UDP DNS module
 *
 * Sets up the UDP DNS service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module.
 *
 * @param config Pointer to the DNS configuration structure
 *
 * @return Handle to the initialized UDP module on success, NULL on failure
 */
esp_dns_handle_t esp_dns_init_udp(esp_dns_config_t *config)
{
    ESP_LOGD(TAG, "Initializing UDP DNS");

    /* Validate parameters */
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration (NULL)");
        return NULL;
    }

    config->protocol = ESP_DNS_PROTOCOL_UDP;

    esp_dns_handle_t handle = esp_dns_init(config);
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize DNS");
        return NULL;
    }

    ESP_LOGD(TAG, "DNS module initialized successfully with protocol DNS Over UDP(%d)", config->protocol);
    return handle;
}


/**
 * @brief Cleans up the UDP DNS module
 *
 * Releases resources allocated for the UDP DNS service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, -1 on failure
 */
int esp_dns_cleanup_udp(esp_dns_handle_t handle)
{
    ESP_LOGD(TAG, "Cleaning up UDP DNS");

    /* Validate parameters */
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle (NULL)");
        return -1;
    }

    if (handle->config.protocol != ESP_DNS_PROTOCOL_UDP) {
        ESP_LOGW(TAG, "Unknown protocol during cleanup: %d", handle->config.protocol);
        return -1;
    }

    int ret = esp_dns_cleanup(handle);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to cleanup DNS");
        return ret;
    }

    /* Empty the handle */
    memset(handle, 0, sizeof(*handle));

    ESP_LOGD(TAG, "DNS module cleaned up DNS Over UDP successfully");
    return 0;
}


/**
 * @brief Resolves a hostname using UDP DNS
 *
 * Performs DNS resolution over UDP for the given hostname. Creates a UDP connection,
 * sends the DNS query, and processes the response.
 *
 * @note This function is a placeholder and does not contain the actual implementation
 *       for UDP DNS resolution. The implementation needs to be added.
 *       As of now the resolution is performed by lwip dns module.
 *
 * @param handle DNS handle
 * @param name Hostname to resolve
 * @param addr Pointer to store the resolved IP address
 * @param rrtype DNS record type
 *
 * @return ERR_OK on success, error code on failure
 */
err_t dns_resolve_udp(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype)
{
    // TBD: Implement UDP DNS resolution
    if (addr == NULL) {
        return ERR_ARG;
    }

    return ERR_OK;
}
