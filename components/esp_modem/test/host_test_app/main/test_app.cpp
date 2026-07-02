/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <memory>
#include <cstdlib>
#include <unistd.h>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "esp_log.h"
#include "vfs_resource/vfs_create.hpp"

using namespace esp_modem;

#ifdef CONFIG_ESP_MODEM_URC_HANDLER
namespace {

struct UrcTestState {
    int urc_lines = 0;
    bool saw_discard_urc = false;
    bool post_consume_all_valid = false;
};

static UrcTestState urc_state;

static DTE::UrcConsumeInfo handle_enhanced_urc(const DTE::UrcBufferInfo &info)
{
    if (info.new_data_size > info.buffer_total_size) {
        return {DTE::UrcConsumeResult::CONSUME_NONE, 0};
    }

    std::string_view buf(reinterpret_cast<const char *>(info.buffer_start), info.buffer_total_size);

    if (urc_state.saw_discard_urc && info.processed_offset == 0 && info.new_data_size > 0) {
        urc_state.post_consume_all_valid = true;
    }

    size_t start = info.processed_offset;
    while (start < info.buffer_total_size) {
        size_t line_end = buf.find('\n', start);
        if (line_end == std::string_view::npos) {
            return {DTE::UrcConsumeResult::CONSUME_NONE, 0};
        }

        std::string_view line = buf.substr(start, line_end - start + 1);
        if (line.rfind("+URCTEST: discard", 0) == 0) {
            urc_state.saw_discard_urc = true;
            return {DTE::UrcConsumeResult::CONSUME_ALL, 0};
        }
        if (line.rfind("+URCTEST:", 0) == 0) {
            urc_state.urc_lines++;
            size_t consume_size = line_end + 1 - info.processed_offset;
            return {DTE::UrcConsumeResult::CONSUME_PARTIAL, consume_size};
        }
        start = line_end + 1;
    }

    return {DTE::UrcConsumeResult::CONSUME_NONE, 0};
}

static command_result wait_for_ok(uint8_t *data, size_t len)
{
    std::string_view resp(reinterpret_cast<char *>(data), len);
    if (resp.find("OK") != std::string_view::npos) {
        return command_result::OK;
    }
    return command_result::TIMEOUT;
}

} // namespace
#endif

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

TEST_CASE("AT commands via socket", "[esp_modem][at]")
{
    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);

    SECTION("set_command_mode") {
        CHECK(dce->set_command_mode() == command_result::OK);
    }

    SECTION("get_signal_quality") {
        int rssi, ber;
        CHECK(dce->get_signal_quality(rssi, ber) == command_result::OK);
        CHECK(rssi == 27);
        CHECK(ber == 99);
    }

    SECTION("get_module_name") {
        std::string model;
        CHECK(dce->get_module_name(model) == command_result::OK);
        CHECK(model == "SimModem-7600");
    }

    SECTION("get_operator_name") {
        std::string operator_name;
        int act = -1;
        CHECK(dce->get_operator_name(operator_name, act) == command_result::OK);
        CHECK(operator_name == "TestOperator");
        CHECK(act == 7);
    }

    SECTION("get_imsi") {
        std::string imsi;
        CHECK(dce->get_imsi(imsi) == command_result::OK);
        CHECK(imsi == "987654321098765");
    }

    SECTION("get_imei") {
        std::string imei;
        CHECK(dce->get_imei(imei) == command_result::OK);
        CHECK(imei == "123456789012345");
    }

    SECTION("read_pin") {
        bool pin_ok;
        CHECK(dce->read_pin(pin_ok) == command_result::OK);
        CHECK(pin_ok == true);
    }
}

#ifdef CONFIG_ESP_MODEM_URC_HANDLER
TEST_CASE("Enhanced URC via socket", "[esp_modem][urc]")
{
    urc_state = {};

    auto dte = create_test_dte();
    REQUIRE(dte != nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    REQUIRE(dce != nullptr);
    dce->set_enhanced_urc(handle_enhanced_urc);

    SECTION("URC before command response") {
        CHECK(dce->command("AT+URCTEST0\r", wait_for_ok, 5000) == command_result::OK);
        CHECK(urc_state.urc_lines == 1);
    }

    SECTION("CONSUME_ALL resets buffer offset") {
        CHECK(dce->command("AT+URCTEST1\r", wait_for_ok, 5000) == command_result::OK);
        CHECK(urc_state.saw_discard_urc);
        CHECK(urc_state.post_consume_all_valid);
    }
}
#endif

int main(int argc, char *argv[])
{
    return Catch::Session().run(argc, argv);
}
