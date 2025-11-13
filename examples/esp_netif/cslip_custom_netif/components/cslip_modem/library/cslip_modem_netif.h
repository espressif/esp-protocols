/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Stop the esp cslip netif
 */
esp_err_t cslip_modem_netif_stop(esp_netif_t *esp_netif);

/**
 * Start the esp cslip netif
 */
esp_err_t cslip_modem_netif_start(esp_netif_t *esp_netif, esp_ip6_addr_t *addr);

/**
 * Write raw packet out the interface
 */
void cslip_modem_netif_raw_write(esp_netif_t *netif, void *buffer, size_t len);
