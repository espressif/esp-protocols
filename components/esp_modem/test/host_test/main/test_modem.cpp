/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#define CATCH_CONFIG_MAIN // This tells the catch header to generate a main
#include <memory>
#include <future>
#include "catch.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "LoopbackTerm.h"

using namespace esp_modem;

TEST_CASE("DTE command races", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>(true);
    auto loopback = term.get();
    auto dte = std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_BG96_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);
    uint8_t resp[] = {'O', 'K', '\n'};
    // run many commands in succession with the timeout set exactly to the timespan of injected reply
    // (checks for potential exception, data races, recycled local variables, etc.)
    for (int i = 0; i < 1000; ++i) {
        loopback->inject(&resp[0], sizeof(resp), sizeof(resp), /* 1ms before injecting reply */1, 0);
        auto ret = dce->command("AT\n", [&](uint8_t *data, size_t len) {
            return command_result::OK;
        }, 1);
        // this command should either timeout or finish successfully
        CHECK((ret == command_result::TIMEOUT || ret == command_result::OK));
    }
}

TEST_CASE("Test polymorphic delete for custom device/dte", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>(true);
    auto dte = std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");

    // Create custom device and DTE manually to check for a potential undefined behaviour
    auto device = new GenericModule(std::move(dte), &dce_config);
    device->power_down();
    delete device;
    auto custom_dte = new DTE(std::make_unique<LoopbackTerm>(false));
    custom_dte->command("AT", nullptr, 0);
    delete custom_dte;
}

TEST_CASE("DCE AT parser", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>(true);
    auto dte =  std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_BG96_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);

    CHECK(dce->set_command_mode() == command_result::OK);

    int milli_volt, bcl, bcs;
    CHECK(dce->get_battery_status(milli_volt, bcl, bcs) == command_result::OK);
    CHECK(milli_volt == 123456);
    CHECK(bcl == 1);
    CHECK(bcs == 20);

    int rssi, ber;
    CHECK(dce->get_signal_quality(rssi, ber) == command_result::OK);
    CHECK(rssi == 123);
    CHECK(ber == 456);

    bool pin_ok;
    CHECK(dce->read_pin(pin_ok) == command_result::OK);
    CHECK(pin_ok == false);
    CHECK(dce->set_pin("1234") == command_result::OK);
    CHECK(dce->read_pin(pin_ok) == command_result::OK);
    CHECK(pin_ok == true);

    std::string model;
    CHECK(dce->get_module_name(model) == command_result::OK);
    CHECK(model == "0G Dummy Model");

    std::string operator_name;
    int act = 99;
    CHECK(dce->get_operator_name(operator_name) == command_result::OK);
    CHECK(operator_name == "OperatorName");
    CHECK(dce->get_operator_name(operator_name, act) == command_result::OK);
    CHECK(operator_name == "OperatorName");
    CHECK(act == 5);
}


TEST_CASE("DTE send/receive command", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto dte =  std::make_unique<DTE>(std::move(term));

    const auto test_command = "Test\n";
    CHECK(term == nullptr);

    CHECK(dte->set_mode(esp_modem::modem_mode::COMMAND_MODE) == true);

    auto ret = dte->command(test_command, [&](uint8_t *data, size_t len) {
        std::string response((char *)data, len);
        CHECK(response == test_command);
        return command_result::OK;
    }, 1000);
    CHECK(ret == command_result::OK);
}


TEST_CASE("DCE commands", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto dte =  std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);

    const auto test_command = "Test\n";
    auto ret = dce->command(test_command, [&](uint8_t *data, size_t len) {
        std::string response((char *)data, len);
        CHECK(response == test_command);
        return command_result::OK;
    }, 1000);
    CHECK(ret == command_result::OK);
}
TEST_CASE("DCE AT commands", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto dte =  std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);

    int milli_volt, bcl, bcs;
    CHECK(dce->set_echo(false) == command_result::OK);
    CHECK(dce->set_echo(true) == command_result::OK);
    CHECK(dce->get_battery_status(milli_volt, bcl, bcs) == command_result::OK);
    CHECK(milli_volt == 123456);
    CHECK(dce->resume_data_mode() == command_result::FAIL);
}


TEST_CASE("DCE modes", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto dte =  std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);

    // UNDER -> CMD (OK)
    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == true);
    // CMD -> CMD (Fail)
    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == false);
    // CMD -> DATA (OK)
    CHECK(dce->set_mode(esp_modem::modem_mode::DATA_MODE) == true);
    // DATA -> CMUX (Fail)
    CHECK(dce->set_mode(esp_modem::modem_mode::CMUX_MODE) == false);
    // DATA back -> CMD (OK)
    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == true);
    // CMD -> CMUX (OK)
    CHECK(dce->set_mode(esp_modem::modem_mode::CMUX_MODE) == true);
    // CMUX -> DATA (Fail)
    CHECK(dce->set_mode(esp_modem::modem_mode::DATA_MODE) == false);
    // CMUX back -> CMD (OK)
    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == true);
}

TEST_CASE("DCE CMUX test", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto dte = std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);

    CHECK(dce->set_mode(esp_modem::modem_mode::CMUX_MODE) == true);
    const auto test_command = "Test\n";
    auto ret = dce->command(test_command, [&](uint8_t *data, size_t len) {
        std::string response((char *) data, len);
        std::cout << "Response:" << response << std::endl;
        CHECK(response == test_command);
        return command_result::OK;
    }, 1000);
    CHECK(ret == command_result::OK);
}

TEST_CASE("Test CMUX protocol by injecting payloads", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto loopback = term.get();
    auto dte = std::make_shared<DTE>(std::move(term));
    CHECK(term == nullptr);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN");
    esp_netif_t netif{};
    auto dce = create_SIM7600_dce(&dce_config, dte, &netif);
    CHECK(dce != nullptr);

    CHECK(dce->set_mode(esp_modem::modem_mode::CMUX_MODE) == true);
    const auto test_command = "Test\n";
    // 1 byte payload size
    uint8_t test_payload[] = {0xf9, 0x09, 0xff, 0x0b, 0x54, 0x65, 0x73, 0x74, 0x0a, 0xbb, 0xf9 };
    loopback->inject(&test_payload[0], sizeof(test_payload), 1);
    auto ret = dce->command(test_command, [&](uint8_t *data, size_t len) {
        std::string response((char *) data, len);
        CHECK(response == test_command);
        return command_result::OK;
    }, 1000);
    CHECK(ret == command_result::OK);

    // 2 byte payload size
    uint8_t long_payload[453] = { 0xf9, 0x09, 0xef, 0x7c, 0x03, 0x7e }; // header
    long_payload[5]   = 0x7e;   // payload to validate
    long_payload[449] = 0x7e;
    long_payload[450] = '\n';
    long_payload[451] = 0x53;   // footer
    long_payload[452] = 0xf9;
    for (int i = 0; i < 5; ++i) {
        // inject the whole payload (i=0) and then per 1,2,3,4 bytes (i)
        loopback->inject(&long_payload[0], sizeof(long_payload), i == 0 ? sizeof(long_payload) : i);
        auto ret = dce->command("ignore", [&](uint8_t *data, size_t len) {
            CHECK(data[0]     == 0x7e);
            CHECK(data[len - 2] == 0x7e);
            CHECK(data[len - 1] == '\n');
            return command_result::OK;
        }, 1000);
        CHECK(ret == command_result::OK);
    }
}
