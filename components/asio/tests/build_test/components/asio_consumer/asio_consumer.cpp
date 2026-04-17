/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "asio.hpp"
#include "asio_consumer.h"

void asio_consumer_run(void)
{
    asio::io_context io_context;
    asio::ip::tcp::socket socket(io_context);
    (void)socket;
}
