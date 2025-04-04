/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_DNS_DEFAULT_TCP_PORT 53    /* Default TCP port for DNS */
#define ESP_DNS_DEFAULT_DOT_PORT 853   /* Default TLS port for DNS over TLS */
#define ESP_DNS_DEFAULT_DOH_PORT 443   /* Default HTTPS port for DNS over HTTPS */

#define ESP_DNS_DEFAULT_TIMEOUT_MS 10000 /* Default timeout for DNS queries in milliseconds */

typedef enum {
    ESP_DNS_PROTOCOL_UDP,           /* Traditional UDP DNS (Port 53) */
    ESP_DNS_PROTOCOL_TCP,           /* TCP DNS (Port 53) */
    ESP_DNS_PROTOCOL_DOT,           /* DNS over TLS (Port 853) */
    ESP_DNS_PROTOCOL_DOH,           /* DNS over HTTPS (Port 443) */
} esp_dns_protocol_type_t;

/**
 * @brief DNS configuration structure
 */
typedef struct {
    /* Basic protocol selection */
    esp_dns_protocol_type_t protocol;  /* DNS protocol type */

    /* Common settings */
    const char *dns_server;            /* DNS server IP address or hostname */
    uint16_t port;                     /* Custom port number (if not using default) */
    uint32_t timeout_ms;               /* Query timeout in milliseconds */

    /* Secure protocol options */
    struct {
        const char *cert_pem;          /* SSL server certification in PEM format as string */
        esp_err_t (*crt_bundle_attach)(void *conf); /* Function pointer to attach cert bundle */
    } tls_config;                      /* Used for DoT, DoH, DoH3, DNSCrypt, DoQ */

    /* Protocol-specific options */
    union {
        /* DoH options */
        struct {
            const char *url_path;      /* URL path for DoH service (e.g., "/dns-query") */
        } doh_config;                  /* DNS over HTTPS configuration */
    } protocol_config;                 /* Protocol-specific configuration */
} esp_dns_config_t;

typedef struct esp_dns_handle* esp_dns_handle_t;

/**
 * @brief Initialize DNS over HTTPS (DoH) module
 *
 * Sets up the DoH service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module.
 *
 * @param config Pointer to the DNS configuration structure
 *
 * @return Handle to the initialized DoH module on success, NULL on failure
 */
esp_dns_handle_t esp_dns_init_doh(esp_dns_config_t *config);

/**
 * @brief Initialize DNS over TLS (DoT) module
 *
 * Sets up the DoT service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module.
 *
 * @param config Pointer to the DNS configuration structure
 *
 * @return Handle to the initialized DoT module on success, NULL on failure
 */
esp_dns_handle_t esp_dns_init_dot(esp_dns_config_t *config);

/**
 * @brief Initialize TCP DNS module
 *
 * Sets up the TCP DNS service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module.
 *
 * @param config Pointer to the DNS configuration structure
 *
 * @return Handle to the initialized TCP module on success, NULL on failure
 */
esp_dns_handle_t esp_dns_init_tcp(esp_dns_config_t *config);

/**
 * @brief Initialize UDP DNS module
 *
 * Sets up the UDP DNS service using the provided configuration. Validates the parameters,
 * sets the protocol, and initializes the DNS module.
 *
 * @param config Pointer to the DNS configuration structure
 *
 * @return Handle to the initialized UDP module on success, NULL on failure
 */
esp_dns_handle_t esp_dns_init_udp(esp_dns_config_t *config);

/**
 * @brief Clean up DNS over HTTPS (DoH) module
 *
 * Releases resources allocated for the DoH service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, -1 on failure
 */
int esp_dns_cleanup_doh(esp_dns_handle_t handle);

/**
 * @brief Clean up DNS over TLS (DoT) module
 *
 * Releases resources allocated for the DoT service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, -1 on failure
 */
int esp_dns_cleanup_dot(esp_dns_handle_t handle);

/**
 * @brief Clean up TCP DNS module
 *
 * Releases resources allocated for the TCP DNS service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, -1 on failure
 */
int esp_dns_cleanup_tcp(esp_dns_handle_t handle);

/**
 * @brief Clean up UDP DNS module
 *
 * Releases resources allocated for the UDP DNS service. Validates the parameters,
 * checks the protocol, and cleans up the DNS module.
 *
 * @param handle Pointer to the DNS handle to be cleaned up
 *
 * @return 0 on success, -1 on failure
 */
int esp_dns_cleanup_udp(esp_dns_handle_t handle);

#ifdef __cplusplus
}
#endif
