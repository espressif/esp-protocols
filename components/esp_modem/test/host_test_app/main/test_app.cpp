/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#define CATCH_CONFIG_MAIN
#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "esp_log.h"
#include "vfs_resource/vfs_create.hpp"

using namespace esp_modem;

[[maybe_unused]] constexpr auto TAG = "host_test_app";

static int get_port()
{
    const char *env = std::getenv("MODEM_SIM_PORT");
    return env ? std::atoi(env) : 10000;
}

static std::shared_ptr<DTE> create_test_dte()
{
    esp_modem_dte_config_t dte_config = {
        .dte_buffer_size = 512,
        .task_stack_size = 4096,
        .task_priority = 5,
        .vfs_config = {}
    };

    struct esp_modem_vfs_socket_creator socket_config = {
        .host_name = "127.0.0.1",
        .port = get_port()
    };

    bool ok = vfs_create_socket(&socket_config, &dte_config.vfs_config);
    if (!ok) {
        return nullptr;
    }
    return create_vfs_dte(&dte_config);
}

TEST_CASE("Basic AT command via socket", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    CHECK(dce->set_command_mode() == command_result::OK);
}

TEST_CASE("Get signal quality", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    int rssi, ber;
    CHECK(dce->get_signal_quality(rssi, ber) == command_result::OK);
    CHECK(rssi == 27);
    CHECK(ber == 99);
}

TEST_CASE("Get module name", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    std::string model;
    CHECK(dce->get_module_name(model) == command_result::OK);
    CHECK(model == "SimModem-7600");
}

TEST_CASE("Get operator name", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    std::string operator_name;
    int act = -1;
    CHECK(dce->get_operator_name(operator_name, act) == command_result::OK);
    CHECK(operator_name == "TestOperator");
    CHECK(act == 7);
}

TEST_CASE("Get IMSI", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    std::string imsi;
    CHECK(dce->get_imsi(imsi) == command_result::OK);
    CHECK(imsi == "987654321098765");
}

TEST_CASE("Get IMEI", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    std::string imei;
    CHECK(dce->get_imei(imei) == command_result::OK);
    CHECK(imei == "123456789012345");
}

TEST_CASE("Read PIN status", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    bool pin_ok;
    CHECK(dce->read_pin(pin_ok) == command_result::OK);
    CHECK(pin_ok == true);
}

#define CATCH_CONFIG_RUNNER
extern "C" int app_main(void)
{
    int argc = 5;
    const char *argv[] = {"host_test_app", "-r", "junit", "-o", "junit.xml", nullptr};

    int result = Catch::Session().run(argc, argv);

    if (result != 0) {
        printf("Test failed with result %d.\n", result);
    } else {
        printf("All tests passed successfully.\n");
    }

    std::exit(result);
}
