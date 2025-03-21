/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * esp_dns.c - Custom DNS module for ESP32 with multiple protocol support
 *
 * Provides DNS resolution capabilities with support for various protocols:
 * - Standard UDP/TCP DNS (Port 53)
 * - DNS over TLS (DoT)
 * - DNS over HTTPS (DoH)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/prot/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "esp_log.h"

#include "esp_dns.h"
#include "esp_dns_utils.h"


/**
 * Opaque handle type for DNS module instances
 */
struct dns_handle {
    /* Configuration */
    esp_dns_config_t config;               /* Copy of user configuration */

    /* Connection state */
    bool initialized;                  /* Flag indicating successful initialization */

    response_buffer_t response_buffer; /* Buffer for storing DNS response data during processing */

    /* Cache management. TBD: Not implemented yet */
    struct {
        void *cache;                   /* DNS cache structure */
        uint64_t last_cleanup;         /* Timestamp of last cache cleanup */
    } cache_state;

    /* Statistics. TBD: Not implemented yet */
    struct {
        uint64_t queries_sent;         /* Total number of queries sent */
        uint64_t successful_responses; /* Total number of successful responses */
        uint64_t timeouts;             /* Number of query timeouts */
        uint64_t errors;               /* Number of query errors */
        uint64_t fallbacks;            /* Number of protocol fallbacks */
        uint64_t cache_hits;           /* Number of cache hits */
        double avg_response_time_ms;   /* Average response time in milliseconds */
    } stats;

    /* Thread safety */
    SemaphoreHandle_t lock;            /* Mutex for synchronization */

    /* Error handling */
    int last_error;                    /* Last error code. TBD: Not used in the code yet */
};

/**
 * Error codes for DNS module
 * TBD: Remove this enum if it's not used
 */
typedef enum {
    DNS_ERR_NONE = 0,
    DNS_ERR_INVALID_PARAM,
    DNS_ERR_MEMORY,
    DNS_ERR_INIT_FAILED,
    DNS_ERR_SOCKET_FAILED,
    DNS_ERR_CONNECTION_FAILED,
    DNS_ERR_PROTOCOL_ERROR,
    DNS_ERR_TLS_ERROR,
    DNS_ERR_HTTP_ERROR,
    DNS_ERR_TIMEOUT,
    DNS_ERR_NOT_IMPLEMENTED
} dns_error_t;
