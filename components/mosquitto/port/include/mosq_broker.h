/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "mosquitto.h"

struct mosquitto__config;

/**
 * @brief Mosquitto configuration structure
 *
 * ESP port of mosquittto supports only the options in this configuration
 * structure.
 */
struct mosq_broker_config {
    char *host; /*!< Address on which the broker is listening for connections */
    int port;   /*!< Port number of the broker to listen to */
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
int mosq_broker_start(struct mosq_broker_config *config);
