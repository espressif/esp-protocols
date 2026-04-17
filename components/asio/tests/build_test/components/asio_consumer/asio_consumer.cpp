/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "asio.hpp"
#include "asio/local/connect_pair.hpp"
#include "asio/local/stream_protocol.hpp"
#include "asio_consumer.h"

void asio_consumer_run(void)
{
    asio::io_context io_context;
    asio::ip::tcp::socket tcp_sock(io_context);
    (void)tcp_sock;

    // Exercise asio::local::connect_pair: this is the only public client-facing
    // API that ultimately calls ::socketpair() (through asio's separately
    // compiled TU). If sock_utils were leaking through asio's public headers,
    // this would require sock_utils on the component's include path.
    asio::local::stream_protocol::socket s1(io_context), s2(io_context);
    asio::error_code ec;
    asio::local::connect_pair(s1, s2, ec);
    (void)ec;
}
