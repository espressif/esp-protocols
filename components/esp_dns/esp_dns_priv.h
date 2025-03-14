/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @file esp_dns_priv.h
 * @brief Private header for ESP DNS module
 *
 * This module provides DNS resolution capabilities with support for various protocols:
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
 * @brief Opaque handle type for DNS module instances
 */
struct esp_dns_handle {
    /* Configuration */
    esp_dns_config_t config;               /* Copy of user configuration */

    /* Connection state */
    bool initialized;                      /* Flag indicating successful initialization */

    response_buffer_t response_buffer;     /* Buffer for storing DNS response data during processing */

    /* Thread safety */
    SemaphoreHandle_t lock;                /* Mutex for synchronization */
};


/**
 * @brief Initialize DNS module with configuration
 *
 * @param config DNS configuration parameters
 *
 * @return esp_dns_handle_t Handle to DNS module instance
 */
esp_dns_handle_t esp_dns_init(const esp_dns_config_t *config);

/**
 * @brief Clean up DNS module resources
 *
 * @param handle DNS module handle
 *
 * @return int 0 on success, negative error code on failure
 */
int esp_dns_cleanup(esp_dns_handle_t handle);

/**
 * @brief Resolve hostname using DNS over HTTPS
 *
 * @param handle DNS module handle
 * @param name Hostname to resolve
 * @param addr Pointer to store resolved IP address
 * @param rrtype Record type (A or AAAA)
 *
 * @return err_t Error code
 */
err_t dns_resolve_doh(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype);

/**
 * @brief Resolve hostname using DNS over TLS
 *
 * @param handle DNS module handle
 * @param name Hostname to resolve
 * @param addr Pointer to store resolved IP address
 * @param rrtype Record type (A or AAAA)
 *
 * @return err_t Error code
 */
err_t dns_resolve_dot(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype);

/**
 * @brief Resolve hostname using TCP DNS
 *
 * @param handle DNS module handle
 * @param name Hostname to resolve
 * @param addr Pointer to store resolved IP address
 * @param rrtype Record type (A or AAAA)
 *
 * @return err_t Error code
 */
err_t dns_resolve_tcp(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype);

/**
 * @brief Resolve hostname using UDP DNS
 *
 * @param handle DNS module handle
 * @param name Hostname to resolve
 * @param addr Pointer to store resolved IP address
 * @param rrtype Record type (A or AAAA)
 *
 * @return err_t Error code
 */
err_t dns_resolve_udp(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype);
