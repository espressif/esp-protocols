/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_tls.h"
#include "sdkconfig.h"
#include "lwip/prot/dns.h"
#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/dns.h"
#include "lwip_default_hooks.h"
#include "esp_http_client.h"
#include "esp_dns_utils.h"
#include "esp_dns_doh.h"

#define TAG "ESP_DNS_DOH"

#define SERVER_URL_MAX_SZ 256

int init_doh_dns(esp_dns_handle_t handle)
{
    ESP_LOGI(TAG, "Initializing DNS over HTTPS");

    return 0;
}

int cleanup_doh_dns(esp_dns_handle_t handle)
{
    ESP_LOGI(TAG, "Cleaning up DNS over HTTPS");

    return 0;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    char *temp_buff = NULL;
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
            parse_dns_response((uint8_t *)handle->response_buffer.buffer,
                               handle->response_buffer.length,
                               &handle->response_buffer.dns_response);
        } else {
            ESP_LOGE(TAG, "HTTP Error: %d", esp_http_client_get_status_code(evt->client));
            ESP_LOG_BUFFER_HEXDUMP(TAG, handle->response_buffer.buffer, handle->response_buffer.length, ESP_LOG_ERROR);
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
    }
    return ESP_OK;
}

/**
 * @brief Function to handle the HTTPS request for DNS resolution.
 *
 * This function generates a DNS request, sends it via HTTPS, and processes
 * the response to extract IP addresses.
 *
 * @param name The name to resolve.
 * @param addr A pointer to an array to store the resolved IP addresses.
 * @param rrtype The address RR type (A or AAAA).
 */
err_t dns_resolve_doh(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype)
{
    uint8_t buffer_qry[BUFFER_SIZE];

    /* Initialize error status */
    err_t err = ERR_OK;
    const char *prefix = "https://";

    /* Set default values for DoH configuration if not specified */
    const char *url_path = handle->config.protocol_config.doh_config.url_path ?
                           handle->config.protocol_config.doh_config.url_path : "dns-query";
    int port = handle->config.port ?
               handle->config.port : DEFAULT_DOH_PORT;

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
        .event_handler = _http_event_handler,
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
    size_t query_size = create_dns_query(buffer_qry, sizeof(buffer_qry), name, rrtype, &handle->response_buffer.dns_response.id);
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
        err = write_ip_addresses_from_dns_response(&handle->response_buffer.dns_response, addr);
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
