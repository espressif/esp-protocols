/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_dns_priv.h"

#ifdef __cplusplus
extern "C" {
#endif


int init_doh_dns(esp_dns_handle_t handle);

int cleanup_doh_dns(esp_dns_handle_t handle);

err_t dns_resolve_doh(const esp_dns_handle_t handle, const char *name, ip_addr_t *addr, u8_t rrtype);

#ifdef __cplusplus
}
#endif
