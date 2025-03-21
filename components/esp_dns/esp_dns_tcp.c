/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_dns_tcp.h"

#define TAG "ESP_DNS_TCP"


int init_tcp_dns(esp_dns_handle_t handle)
{
    ESP_LOGI(TAG, "Initializing TCP DNS");

    return 0;
}

int cleanup_tcp_dns(esp_dns_handle_t handle)
{
    ESP_LOGI(TAG, "Cleaning up TCP DNS");

    return 0;
}

err_t dns_resolve_tcp(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype)
{
    int err = ERR_OK;
    esp_transport_handle_t transport = NULL;
    int len = 0;
    char tcp_buffer[BUFFER_SIZE];
    size_t query_size;
    int timeout_ms;
    int tcp_port;

    if (addr == NULL) {
        return ERR_ARG;
    }

    /* Set timeout and port values, using defaults if not specified in config */
    timeout_ms = handle->config.timeout_ms ? : DEFAULT_TIMEOUT_MS;
    tcp_port = handle->config.port ? : DEFAULT_TCP_PORT;

    /* Clear the response buffer to ensure no residual data remains */
    memset(&handle->response_buffer, 0, sizeof(response_buffer_t));

    /* Create DNS query in wire format, leaving 2 bytes at start for length prefix as required by RFC 7858 */
    memset(tcp_buffer, 0, BUFFER_SIZE);
    query_size = create_dns_query((uint8_t *)(tcp_buffer + 2), sizeof(tcp_buffer) - 2,
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
        ESP_LOGE(TAG, "TLS connection failed");
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
    memset(tcp_buffer, 0, BUFFER_SIZE);
    len = esp_transport_read(transport,
                             tcp_buffer,
                             sizeof(tcp_buffer),
                             timeout_ms);
    if (len > 0) {
        /* Skip the 2-byte length field that prepends DNS messages as required by RFC 7858 */
        handle->response_buffer.buffer = tcp_buffer + 2;
        handle->response_buffer.length = len - 2;

        /* Parse the DNS response */
        parse_dns_response((uint8_t *)handle->response_buffer.buffer,
                           handle->response_buffer.length,
                           &handle->response_buffer.dns_response);

        /* Extract IP addresses from DNS response */
        err = write_ip_addresses_from_dns_response(&handle->response_buffer.dns_response, addr);
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
