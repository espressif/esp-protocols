/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "unity.h"
extern "C"  // workaround for unity headers (possibly) without C++ guards
{
#include "unity_fixture.h"
}
#include "test_utils.h"
#include "memory_checks.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "asio.hpp"
#include "asio/ssl.hpp"

static void create_stream_and_attempt_handshake()
{
    asio::io_context io;
    asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
    asio::ssl::stream<asio::ip::tcp::socket> stream(io, ctx);
    stream.set_verify_mode(asio::ssl::verify_none);

    asio::error_code ec;
    stream.handshake(asio::ssl::stream_base::client, ec);
}


TEST_GROUP(asio_ssl);

TEST_SETUP(asio_ssl)
{
    // call once to allocate all one time inits
    create_stream_and_attempt_handshake();
    test_utils_record_free_mem();
}

TEST_TEAR_DOWN(asio_ssl)
{
    // account for some timer-based lwip allocations
    test_utils_finish_and_evaluate_leaks(128, 256);
}


TEST(asio_ssl, ssl_stream_lifecycle_no_leak)
{
    test_case_uses_tcpip();
    create_stream_and_attempt_handshake();
    TEST_ASSERT_TRUE(true);
}

TEST_GROUP_RUNNER(asio_ssl)
{
    RUN_TEST_CASE(asio_ssl, ssl_stream_lifecycle_no_leak)
}

extern "C" void app_main(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    UNITY_MAIN(asio_ssl);
}
