/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "mosq_broker.h"

TEST_CASE("Start and stop mosquitto broker", "[mosquitto]")
{
    struct mosq_broker_config config = {};
    config.host = "0.0.0.0";
    config.port = 18833;

    int broker_rc = -1;
    std::thread broker_thread([&]() {
        broker_rc = mosq_broker_run(&config);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    mosq_broker_stop();

    broker_thread.join();

    CHECK(broker_rc == 0);
}

TEST_CASE("Restart mosquitto broker after stop", "[mosquitto]")
{
    struct mosq_broker_config config = {};
    config.host = "0.0.0.0";
    config.port = 18834;

    auto run_broker = [&]() {
        int broker_rc = -1;
        std::thread broker_thread([&]() {
            broker_rc = mosq_broker_run(&config);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        mosq_broker_stop();

        broker_thread.join();

        return broker_rc;
    };

    CHECK(run_broker() == 0);
    CHECK(run_broker() == 0);
}

extern "C" void app_main(void)
{
    int result = Catch::Session().run();
    if (result != 0) {
        printf("Test failed with result %d.\n", result);
    } else {
        printf("All tests passed successfully.\n");
    }
    std::exit(result);
}
