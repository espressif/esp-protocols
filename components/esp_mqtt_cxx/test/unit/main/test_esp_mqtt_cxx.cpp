/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "catch2/catch_session.hpp"
#include "catch2/catch_test_macros.hpp"
#include "esp_mqtt.hpp"
#include "esp_mqtt_client_config.hpp"
#include "esp_netif.h"

namespace mqtt = idf::mqtt;

namespace {
class TestClient final : public mqtt::Client {
public:
    using mqtt::Client::Client;

    bool constructed{false};
    bool before_connect{false};
    bool disconnected{false};

    TestClient(const mqtt::BrokerConfiguration &broker, const mqtt::ClientCredentials &credentials, const mqtt::Configuration &config) :
        mqtt::Client(broker, credentials, config)
    {
        constructed = true;
    }

private:
    void on_connected(esp_mqtt_event_handle_t const event) override
    {
        CHECK(constructed);
    }

    void on_data(esp_mqtt_event_handle_t const event) override
    {
        CHECK(constructed);
    }

    void on_before_connect(esp_mqtt_event_handle_t const event) override
    {
        CHECK(constructed);
        before_connect = true;
    }

    void on_disconnected(const esp_mqtt_event_handle_t event) override
    {
        CHECK(constructed);
        disconnected = true;

    }

};
} // namespace

TEST_CASE("Client does not auto-start and can dispatch events after construction", "[esp_mqtt_cxx]")
{
    mqtt::BrokerConfiguration broker{
        .address = mqtt::URI{std::string{"mqtt://127.0.0.1:1883"}},
        .security = mqtt::Insecure{}
    };
    mqtt::ClientCredentials credentials{};
    mqtt::Configuration config{};

    TestClient client{broker, credentials, config};

    REQUIRE(client.is_started() == false);

    // start the client and expect disconnection (reset by peer)
    // since no server's running on this ESP32
    client.start();
    CHECK(client.is_started() == true);

    CHECK(client.before_connect);
    usleep(10000);
    CHECK(client.disconnected);
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    Catch::Session session;

    int failures = session.run();
    if (failures > 0) {
        printf("TEST FAILED! number of failures=%d\n", failures);
        return;
    }
    printf("Test passed!\n");
}
