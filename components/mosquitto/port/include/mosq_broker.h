/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "mosquitto.h"

struct mosquitto__config;

struct mosq_broker_config {
    char *host;
    int port;
};

int run_broker(struct mosq_broker_config *config);
