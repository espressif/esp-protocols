/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_dns_priv.h"
#include "esp_dns.h"

#define TAG "ESP_DNS_TCP"

/**
 * @brief Initializes the TCP DNS module
 *
 * Sets up the TCP DNS service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module.
 *
 * @param config Pointer to the DNS configuration structure
 *
 * @return Handle to the initialized TCP module on success, NULL on failure
 */
esp_dns_handle_t esp_dns_init_tcp(esp_dns_config_t *config)
{
    ESP_LOGD(TAG, "Initializing TCP DNS");

    /* Validate parameters */
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration (NULL)");
        return NULL;
    }

    config->protocol = ESP_DNS_PROTOCOL_TCP;

    esp_dns_handle_t handle = esp_dns_init(config);
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize DNS");
        return NULL;
    }

    ESP_LOGD(TAG, "DNS module initialized successfully with protocol DNS Over TCP(%d)", config->protocol);
    return handle;
}

/**
 * @brief Cleans up the TCP DNS module
 *
 * Releases resources allocated for the TCP DNS service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, -1 on failure
 */
int esp_dns_cleanup_tcp(esp_dns_handle_t handle)
{
    ESP_LOGD(TAG, "Cleaning up TCP DNS");

    /* Validate parameters */
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle (NULL)");
        return -1;
    }

    if (handle->config.protocol != ESP_DNS_PROTOCOL_TCP) {
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

    ESP_LOGD(TAG, "DNS module cleaned up DNS Over TCP successfully");
    return 0;
}

/**
 * @brief Resolves a hostname using TCP DNS
 *
 * Performs DNS resolution over TCP for the given hostname. Creates a TCP connection,
 * sends the DNS query, and processes the response.
 *
 * @param handle DNS handle
 * @param name Hostname to resolve
 * @param addr Pointer to store the resolved IP address
 * @param rrtype DNS record type
 *
 * @return ERR_OK on success, error code on failure
 */
err_t dns_resolve_tcp(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype)
{
    int err = ERR_OK;
    esp_transport_handle_t transport = NULL;
    int len = 0;
    char tcp_buffer[ESP_DNS_BUFFER_SIZE];
    size_t query_size;
    int timeout_ms;
    int tcp_port;

    if (addr == NULL) {
        return ERR_ARG;
    }

    /* Set timeout and port values, using defaults if not specified in config */
    timeout_ms = handle->config.timeout_ms ? : ESP_DNS_DEFAULT_TIMEOUT_MS;
    tcp_port = handle->config.port ? : ESP_DNS_DEFAULT_TCP_PORT;

    /* Clear the response buffer to ensure no residual data remains */
    memset(&handle->response_buffer, 0, sizeof(response_buffer_t));

    /* Create DNS query in wire format, leaving 2 bytes at start for length prefix as required by RFC 7858 */
    memset(tcp_buffer, 0, ESP_DNS_BUFFER_SIZE);
    query_size = esp_dns_create_query((uint8_t *)(tcp_buffer + 2), sizeof(tcp_buffer) - 2,
                                      name, rrtype, &handle->response_buffer.dns_response.id);
    if (query_size == -1) {
        ESP_LOGE(TAG, "Error: Hostname too big");
        return ERR_MEM;
    }

    /* Prepends the 2-byte length field to DNS messages as required by RFC 7858 */
    tcp_buffer[0] = (query_size >> 8) & 0xFF;
    tcp_buffer[1] = query_size & 0xFF;

    transport = esp_transport_tcp_init();
    if (!transport) {
        ESP_LOGE(TAG, "Failed to initialize transport");
        return ERR_MEM;
    }

    if (esp_transport_connect(transport, handle->config.dns_server, tcp_port, timeout_ms) < 0) {
        ESP_LOGE(TAG, "TCP connection failed");
        err = ERR_CONN;
        goto cleanup;
    }

    /* Send DNS query */
    len = esp_transport_write(transport,
                              tcp_buffer,
                              query_size + 2,
                              timeout_ms);
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to send DNS query");
        err = ERR_ABRT;
        goto cleanup;
    }

    /* Read response */
    memset(tcp_buffer, 0, ESP_DNS_BUFFER_SIZE);
    len = esp_transport_read(transport,
                             tcp_buffer,
                             sizeof(tcp_buffer),
                             timeout_ms);
    if (len > 0) {
        /* Skip the 2-byte length field that prepends DNS messages as required by RFC 7858 */
        handle->response_buffer.buffer = tcp_buffer + 2;
        handle->response_buffer.length = len - 2;

        /* Parse the DNS response */
        esp_dns_parse_response((uint8_t *)handle->response_buffer.buffer,
                               handle->response_buffer.length,
                               &handle->response_buffer.dns_response);

        /* Extract IP addresses from DNS response */
        err = esp_dns_extract_ip_addresses_from_response(&handle->response_buffer.dns_response, addr);
        if (err != ERR_OK) {
            ESP_LOGE(TAG, "Failed to extract IP address from DNS response");
            goto cleanup;
        }
    } else {
        ESP_LOGE(TAG, "Failed to receive response");
        err = ERR_ABRT;
        goto cleanup;
    }

cleanup:
    if (transport) {
        esp_transport_close(transport);
        esp_transport_destroy(transport);
    }

    return err;
}
