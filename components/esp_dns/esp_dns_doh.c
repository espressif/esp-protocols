/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "esp_dns_utils.h"
#include "esp_dns_priv.h"
#include "esp_dns.h"

#define TAG "ESP_DNS_DOH"

#define SERVER_URL_MAX_SZ 256

/**
 * @brief Initializes the DNS over HTTPS (DoH) module
 *
 * Sets up the DoH service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module. Returns a handle for further use.
 *
 * @param config Pointer to the DNS configuration structure, which must be initialized
 *
 * @return On success, returns a handle to the initialized DoH module; returns NULL on failure
 */
esp_dns_handle_t esp_dns_init_doh(esp_dns_config_t *config)
{
    ESP_LOGD(TAG, "Initializing DNS over HTTPS");

    /* Validate parameters */
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid configuration (NULL)");
        return NULL;
    }

    config->protocol = ESP_DNS_PROTOCOL_DOH;

    esp_dns_handle_t handle = esp_dns_init(config);
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize DNS");
        return NULL;
    }

    ESP_LOGD(TAG, "DNS module initialized successfully with protocol DNS Over HTTPS(%d)", config->protocol);
    return handle;
}

/**
 * @brief Cleans up the DNS over HTTPS (DoH) module
 *
 * Releases resources allocated for the DoH service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, or -1 on failure
 */
int esp_dns_cleanup_doh(esp_dns_handle_t handle)
{
    ESP_LOGD(TAG, "Cleaning up DNS over HTTPS");

    /* Validate parameters */
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid handle (NULL)");
        return -1;
    }

    if (handle->config.protocol != ESP_DNS_PROTOCOL_DOH) {
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

    ESP_LOGD(TAG, "DNS module cleaned up DNS Over HTTPS successfully");
    return 0;
}

/**
 * @brief HTTP event handler for DNS over HTTPS requests
 *
 * Handles HTTP events during DNS over HTTPS communication, including data reception,
 * connection status, and error conditions.
 *
 * @param evt Pointer to the HTTP client event structure
 *
 * @return ESP_OK on success, or an error code on failure
 */
esp_err_t esp_dns_http_event_handler(esp_http_client_event_t *evt)
{
    char *temp_buff = NULL;
    size_t temp_buff_len = 0;
    esp_dns_handle_t handle = (esp_dns_handle_t)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /* Check if buffer is null, if yes, initialize it */
        if (handle->response_buffer.buffer == NULL) {
            if (evt->data_len == 0) {
                ESP_LOGW(TAG, "Received empty HTTP data");
                return ESP_ERR_INVALID_ARG;
            }
            temp_buff = malloc(evt->data_len);
            if (temp_buff) {
                handle->response_buffer.buffer = temp_buff;
                handle->response_buffer.length = evt->data_len;
                memcpy(handle->response_buffer.buffer, evt->data, evt->data_len);
            } else {
                ESP_LOGE(TAG, "Buffer allocation error");
                return ESP_ERR_NO_MEM;
            }
        } else {
            /* Reallocate buffer to hold the new data chunk */
            int new_len = handle->response_buffer.length + evt->data_len;
            if (new_len == 0) {
                ESP_LOGW(TAG, "New data length is zero after receiving HTTP data");
                return ESP_ERR_INVALID_ARG;
            }
            temp_buff = realloc(handle->response_buffer.buffer, new_len);
            if (temp_buff) {
                handle->response_buffer.buffer = temp_buff;
                memcpy(handle->response_buffer.buffer + handle->response_buffer.length, evt->data, evt->data_len);
                handle->response_buffer.length = new_len;
            } else {
                ESP_LOGE(TAG, "Buffer allocation error");
                return ESP_ERR_NO_MEM;
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        /* Entire response received, process it here */
        ESP_LOGD(TAG, "Received full response, length: %d", handle->response_buffer.length);

        /* Check if the buffer indicates an HTTP error response */
        if (HttpStatus_Ok == esp_http_client_get_status_code(evt->client)) {
            /* Parse the DNS response */
            esp_dns_parse_response((uint8_t *)handle->response_buffer.buffer,
                                   handle->response_buffer.length,
                                   &handle->response_buffer.dns_response);
        } else {
            ESP_LOGE(TAG, "HTTP Error: %d", esp_http_client_get_status_code(evt->client));
            temp_buff_len = handle->response_buffer.length > ESP_DNS_BUFFER_SIZE ? ESP_DNS_BUFFER_SIZE : handle->response_buffer.length;
            ESP_LOG_BUFFER_HEXDUMP(TAG, handle->response_buffer.buffer, temp_buff_len, ESP_LOG_ERROR);
            handle->response_buffer.dns_response.status_code = ERR_VAL;    /* TBD: Not handled properly yet */
        }

        free(handle->response_buffer.buffer);
        handle->response_buffer.buffer = NULL;
        handle->response_buffer.length = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGE(TAG, "HTTP_EVENT_REDIRECT: Not supported(%d)", esp_http_client_get_status_code(evt->client));
        break;
    default:
        ESP_LOGD(TAG, "Other HTTP event: %d", evt->event_id);
        break;
    }
    return ESP_OK;
}

/**
 * @brief Resolves a hostname using DNS over HTTPS
 *
 * This function generates a DNS request, sends it via HTTPS, and processes
 * the response to extract IP addresses.
 *
 * @param handle Pointer to the DNS handle
 * @param name The hostname to resolve
 * @param addr Pointer to store the resolved IP addresses
 * @param rrtype The address RR type (A or AAAA)
 *
 * @return ERR_OK on success, or an error code on failure
 */
err_t dns_resolve_doh(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype)
{
    uint8_t buffer_qry[ESP_DNS_BUFFER_SIZE];

    /* Initialize error status */
    err_t err = ERR_OK;
    const char *prefix = "https://";

    /* Set default values for DoH configuration if not specified */
    const char *url_path = handle->config.protocol_config.doh_config.url_path ?
                           handle->config.protocol_config.doh_config.url_path : "dns-query";
    int port = handle->config.port ?
               handle->config.port : ESP_DNS_DEFAULT_DOH_PORT;

    /* Calculate required URL length: https:// + server + / + path + null terminator */
    size_t url_len = strlen(prefix) + \
                     strlen(handle->config.dns_server) + 1 + \
                     strlen(url_path) + 1;  /* 1 for '/' and 1 for '\0' */

    /* Allocate memory for the full server URL */
    char *dns_server_url = malloc(url_len);
    if (dns_server_url == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ERR_MEM;
    }

    /* Construct the complete server URL by combining prefix, server and path */
    snprintf(dns_server_url, url_len, "%s%s/%s", prefix,
             handle->config.dns_server,
             url_path);

    /* Configure the HTTP client with base settings */
    esp_http_client_config_t config = {
        .url = dns_server_url,
        .event_handler = esp_dns_http_event_handler,
        .method = HTTP_METHOD_POST,
        .user_data = handle,
        .port = port,
    };

    /* Configure TLS certificate settings - either using bundle or PEM cert */
    if (handle->config.tls_config.crt_bundle_attach) {
        config.crt_bundle_attach = handle->config.tls_config.crt_bundle_attach;
    } else {
        config.cert_pem = handle->config.tls_config.cert_pem;  /* Use the root certificate for dns.google if needed */
    }

    /* Clear the response buffer to ensure no residual data remains */
    memset(&handle->response_buffer, 0, sizeof(response_buffer_t));

    /* Create DNS query in wire format */
    size_t query_size = esp_dns_create_query(buffer_qry, sizeof(buffer_qry), name, rrtype, &handle->response_buffer.dns_response.id);
    if (query_size == -1) {
        ESP_LOGE(TAG, "Error: Hostname too big");
        err = ERR_MEM;
        goto cleanup;
    }

    /* Initialize HTTP client with the configuration */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Error initializing HTTP client");
        err = ERR_VAL;
        goto cleanup;
    }

    /* Set Content-Type header for DNS-over-HTTPS */
    esp_err_t ret = esp_http_client_set_header(client, "Content-Type", "application/dns-message");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error setting HTTP header: %s", esp_err_to_name(ret));
        err = ERR_VAL;
        goto client_cleanup;
    }

    /* Set the DNS query as POST data */
    ret = esp_http_client_set_post_field(client, (const char *)buffer_qry, query_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error setting POST field: %s", esp_err_to_name(ret));
        err = ERR_VAL;
        goto client_cleanup;
    }

    /* Execute the HTTP request */
    ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        /* Verify HTTP status code and DNS response status */
        if ((HttpStatus_Ok != esp_http_client_get_status_code(client)) ||
                (handle->response_buffer.dns_response.status_code != ERR_OK)) {
            err = ERR_ARG;
            goto client_cleanup;
        }

        /* Extract IP addresses from DNS response */
        err = esp_dns_extract_ip_addresses_from_response(&handle->response_buffer.dns_response, addr);
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(ret));
        err = ERR_VAL;
    }

    /* Clean up HTTP client */
client_cleanup:
    esp_http_client_cleanup(client);

    /* Free allocated memory */
cleanup:
    free(dns_server_url);

    return err;
}
