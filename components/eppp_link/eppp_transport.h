/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

esp_err_t eppp_transport_init(eppp_config_t *config, esp_netif_t *esp_netif);

esp_err_t eppp_transport_tx(void *h, void *buffer, size_t len);

esp_err_t eppp_transport_rx(esp_netif_t *netif);

void eppp_transport_deinit(void);
