/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "mosquitto.h"
#include "esp_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

struct mosquitto__config;

typedef void (*mosq_message_cb_t)(char *client, char *topic, char *data, int len, int qos, int retain);

typedef int (*mosq_connect_cb_t)(const char *client_id, const char *username, const char *password, int password_len);
/**
 * @brief Mosquitto configuration structure
 *
 * ESP port of mosquittto supports only the options in this configuration
 * structure.
 */
struct mosq_broker_config {
    const char *host; /*!< Address on which the broker is listening for connections */
    int port;   /*!< Port number of the broker to listen to */
    esp_tls_cfg_server_t *tls_cfg;  /*!< ESP-TLS configuration (if TLS transport used)
                                     * Please refer to the ESP-TLS official documentation
                                     * for more details on configuring the TLS options.
                                     * You can open the respective docs with this idf.py command:
                                     * `idf.py docs -sp api-reference/protocols/esp_tls.html`
                                     */
    void (*handle_message_cb)(char *client, char *topic, char *data, int len, int qos, int retain); /*!<
                                     * On message callback. If configured, user function is called
                                     * whenever mosquitto processes a message.
                                     */
    mosq_connect_cb_t handle_connect_cb; /*!< On connect callback. If configured, user function is called
                                     * whenever a client attempts to connect. The callback receives
                                     * client_id, username, password, and password length. Return 0 to
                                     * accept the connection, non-zero to reject it.
                                     */
};

/**
 * @brief Start mosquitto broker
 *
 * This API runs the broker in the calling thread and blocks until
 * the mosquitto exits.
 *
 * @param config Mosquitto configuration structure
 * @return int Exit code (0 on success)
 */
int mosq_broker_run(struct mosq_broker_config *config);

/**
 * @brief Stops running broker
 *
 * @note After calling this API, function mosq_broker_run() unblocks and returns.
 */
void mosq_broker_stop(void);

#ifdef __cplusplus
}
#endif
