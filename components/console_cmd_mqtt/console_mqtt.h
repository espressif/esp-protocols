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
 * @brief Set MQTT client certificate
 *
 *  This function sets the MQTT client certificate for secure communication.
 *  The function takes the PEM(Privacy Enhanced Mail) encoded certificate arguments.
 *
 * @param client_cert_pem_start_i Pointer to the beginning of the client certificate PEM data.
 * @param client_cert_pem_end_i Pointer to the end of the client certificate PEM data.
 *
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t set_mqtt_client_cert(const uint8_t *client_cert_pem_start_i, const uint8_t *client_cert_pem_end_i);


/**
 * @brief Set MQTT client key
 *
 *  This function sets the MQTT client key for secure communication.
 *  The function takes the PEM(Privacy Enhanced Mail) encoded key arguments.
 *
 * @param client_key_pem_start_i Pointer to the beginning of the client key PEM data.
 * @param client_key_pem_end_i Pointer to the end of the client key PEM data.
 *
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t set_mqtt_client_key(const uint8_t *client_key_pem_start_i, const uint8_t *client_key_pem_end_i);


/**
 * @brief Set MQTT broker certificate
 *
 *  This function sets the MQTT broker certificate for secure communication.
 *  The function takes the PEM(Privacy Enhanced Mail) encoded broker certificate arguments.
 *
 * @param broker_cert_pem_start_i Pointer to the beginning of the broker certificate PEM data.
 * @param broker_cert_pem_end_i Pointer to the end of the broker certificate PEM data.
 *
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG on invalid arguments
 */
esp_err_t set_mqtt_broker_certs(const uint8_t *broker_cert_pem_start_i, const uint8_t *broker_cert_pem_end_i);

#ifdef __cplusplus
}
#endif
