/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_netif.h"
#include "driver/uart.h"

/**
 * Minimal CSLIP-facing default netif config (mirrors SLIP example)
 */
#define ESP_NETIF_INHERENT_DEFAULT_CSLIP() \
    {   \
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac) \
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
        .get_ip_event = 0,    \
        .lost_ip_event = 0,   \
        .if_key = "CSLP_DEF", \
        .if_desc = "cslip",   \
        .route_prio = 16,     \
        .bridge_info = NULL   \
}

extern const esp_netif_netstack_config_t *netstack_default_cslip;

typedef struct cslip_modem *cslip_modem_handle;

// Optional filter for application-specific serial messages in the stream
typedef bool cslip_rx_filter_cb_t(cslip_modem_handle cslip, uint8_t *data, uint32_t len);

// Minimal CSLIP config stub per requirements (values currently unused)
typedef struct {
    bool    enable;               // default true
    uint8_t vj_slots;             // default 16
    bool    slotid_compression;   // default true
    bool    safe_mode;            // default true
} esp_cslip_config_t;

// Configuration structure for CSLIP modem interface (extends SLIP modem config)
typedef struct {
    uart_port_t uart_dev; /* UART device for reading/writing */

    int uart_tx_pin;      /* UART TX pin number */
    int uart_rx_pin;      /* UART RX pin number */

    uint32_t uart_baud;   /* UART baud rate */

    uint32_t rx_buffer_len;           /* RX buffer length */

    cslip_rx_filter_cb_t *rx_filter;  /* Optional RX filter */
    esp_ip6_addr_t *ipv6_addr;        /* IPv6 address */

    esp_cslip_config_t cslip;         /* CSLIP options (currently informational) */
} cslip_modem_config_t;

// Create a CSLIP modem (initially pass-through SLIP behavior)
cslip_modem_handle cslip_modem_create(esp_netif_t *cslip_netif, const cslip_modem_config_t *modem_config);

// Destroy a CSLIP modem
esp_err_t cslip_modem_destroy(cslip_modem_handle cslip);

// Get configured IPv6 address
esp_ip6_addr_t cslip_modem_get_ipv6_address(cslip_modem_handle cslip);

// Raw write out the interface
void cslip_modem_raw_write(cslip_modem_handle cslip, void *buffer, size_t len);
