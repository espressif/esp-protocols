/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "eppp_link.h"

eppp_transport_handle_t eppp_uart_init(struct eppp_config_uart_s *config);
void eppp_uart_deinit(eppp_transport_handle_t h);
