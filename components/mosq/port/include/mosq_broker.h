/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "mosquitto.h"
#include "esp_tls.h"

struct mosquitto__config;

struct mosq_broker_config {
    char *host;
    int port;
    esp_tls_cfg_server_t *tls_cfg;
};

/**
 * @brief Starts mosquitto broker and runs it in the current task
 * @param config Configuration structure
 * @return 0 (MOSQ_ERR_SUCCESS) on success (when broker stops running)
 */
int run_broker(struct mosq_broker_config *config);

/**
 * @brief Stops running broker
 *
 * @note After calling this API, function run_broker() unblocks and returns.
 */
void stop_broker(void);
