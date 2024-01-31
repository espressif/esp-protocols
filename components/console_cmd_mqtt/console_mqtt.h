/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "console_simple_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the mqtt command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_mqtt_register(void);


/**
 * @brief Set MQTT client certificates
 *
 *  This function sets the MQTT client certificates for secure communication.
 *  The function takes the PEM(Privacy Enhanced Mail) encoded certificates and keys as arguments.
 *
 * @param client_cert_pem_start_i Pointer to the beginning of the client certificate PEM data.
 * @param client_cert_pem_end_i Pointer to the end of the client certificate PEM data.
 * @param client_key_pem_start_i Pointer to the beginning of the client key PEM data.
 * @param client_key_pem_end_i Pointer to the end of the client key PEM data.
 * @param server_cert_pem_start_i Pointer to the beginning of the server certificate PEM data.
 * @param server_cert_pem_end_i Pointer to the end of the server certificate PEM data.
 *
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t set_mqtt_certs(const uint8_t *client_cert_pem_start_i, const uint8_t *client_cert_pem_end_i,
                         const uint8_t *client_key_pem_start_i, const uint8_t *client_key_pem_end_i,
                         const uint8_t *server_cert_pem_start_i, const uint8_t *server_cert_pem_end_i);

#ifdef __cplusplus
}
#endif
