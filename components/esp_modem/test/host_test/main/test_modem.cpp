#define CATCH_CONFIG_MAIN // This tells the catch header to generate a main
#include <memory>
#include <future>
#include "catch.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "LoopbackTerm.h"

using namespace esp_modem;

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

    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == true);
    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == false);
    CHECK(dce->set_mode(esp_modem::modem_mode::DATA_MODE) == true);
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
