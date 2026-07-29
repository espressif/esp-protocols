/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup ESP_MODEM_CONFIG
 * @{
 */

/**
 * @brief ESP Modem DCE Default Configuration
 *
 * Defaults: PDP context id 1, protocol type "IP" (IPv4).
 * Override context_id / protocol_type for other carriers or IP stacks,
 * e.g. context_id=3 (Verizon), protocol_type="IPV6" or "IPV4V6".
 */
#define ESP_MODEM_DCE_DEFAULT_CONFIG(APN)       \
    {                                           \
        .apn = APN,                             \
        .context_id = 1,                        \
        .protocol_type = "IP"                   \
    }

typedef struct esp_modem_dce_config esp_modem_dce_config_t;

/**
 * @brief DCE configuration structure
 *
 * Holds the PDP context parameters used when entering data mode
 * (AT+CGDCONT). Zero-initialized context_id / NULL protocol_type
 * fall back to defaults (1 / "IP").
 */
struct esp_modem_dce_config {
    const char *apn;            /*!< APN: Logical name of the Access point */
    size_t context_id;          /*!< PDP context id (CID), typically 1; some carriers use 3 */
    const char *protocol_type;  /*!< PDP type: "IP", "IPV6", or "IPV4V6" */
};

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
