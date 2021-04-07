#define CATCH_CONFIG_MAIN // This tells the catch header to generate a main
#include <memory>
#include <future>
#include "catch.hpp"
#include "cxx_include/esp_modem_terminal.hpp"
#include "cxx_include/esp_modem_api.hpp"

using namespace esp_modem;

class LoopbackTerm : public Terminal {
public:
    explicit LoopbackTerm(): loopback_data(), data_len(0) {}

    ~LoopbackTerm() override = default;

    void start() override {
        status = status_t::STARTED;
    }

    void stop() override {
        status = status_t::STOPPED;
    }

    int write(uint8_t *data, size_t len) override {
        if (len > 2 && (data[len-1] == '\r' || data[len-1] == '+') ) { // Simple AT responder
            std::string command((char*)data, len);
            std::string response;
            if (command == "ATE1\r" || command == "ATE0\r" || command == "+++") {
                response = "OK\r\n";
            } else if (command == "ATO\r") {
                response = "ERROR\r\n";
            } else if (command.find("ATD") != std::string::npos) {
                response = "CONNECT\r\n";
            } else if (command.find("AT") != std::string::npos) {
                response = "OK\r\n";
            }
            if (!response.empty()) {
                data_len = response.length();
                loopback_data.resize(data_len);
                memcpy(&loopback_data[0], &response[0], data_len);
                auto ret = std::async(on_data, nullptr, data_len);
                return len;
            }
        }
        if (len > 2 && data[0] == 0xf9 && data[2] == 0xef) { // Simple CMUX responder
            data[2] = 0xff; // turn the request into a reply -> implements CMUX loopback
        }
        loopback_data.resize(data_len + len);
        memcpy(&loopback_data[data_len], data, len);
        data_len += len;
        auto ret = std::async(on_data, nullptr, data_len);
        return len;
    }

    int read(uint8_t *data, size_t len) override {
        size_t read_len = std::min(data_len, len);
        if (read_len) {
            memcpy(data, &loopback_data[0], len);
            loopback_data.erase(loopback_data.begin(), loopback_data.begin() + read_len);
            data_len -= len;
        }
        return read_len;
    }


private:
    enum class status_t {
        STARTED,
        STOPPED
    };
    status_t status;
    signal_group signal;
    std::vector<uint8_t> loopback_data;
    size_t data_len;
};

TEST_CASE("DTE send/receive command", "[esp_modem]")
{
    auto term = std::make_unique<LoopbackTerm>();
    auto dte =  std::make_unique<DTE>(std::move(term));

    const auto test_command = "Test\n";
    CHECK(term == nullptr);

    dte->set_mode(esp_modem::modem_mode::COMMAND_MODE);

    auto ret = dte->command(test_command, [&](uint8_t *data, size_t len) {
        std::string response((char*)data, len);
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
        std::string response((char*)data, len);
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

    CHECK(dce->set_echo(false) == command_result::OK);
    CHECK(dce->set_echo(true) == command_result::OK);
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

    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == false);
    CHECK(dce->set_mode(esp_modem::modem_mode::DATA_MODE) == true);
    CHECK(dce->set_mode(esp_modem::modem_mode::COMMAND_MODE) == true);
}

TEST_CASE("DCE CMUX test", "[esp_modem]") {
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
        CHECK(response == test_command);
        return command_result::OK;
    }, 1000);
    CHECK(ret == command_result::OK);
}