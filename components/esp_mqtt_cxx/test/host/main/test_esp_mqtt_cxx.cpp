/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "catch2/catch_session.hpp"
#include "catch2/catch_test_macros.hpp"
#include "esp_mqtt.hpp"
#include "esp_mqtt_client_config.hpp"

namespace mqtt = idf::mqtt;

namespace {
class TestClient final : public mqtt::Client {
public:
    using mqtt::Client::Client;

    bool constructed{false};
    bool connected_called{false};

    TestClient(const mqtt::BrokerConfiguration &broker, const mqtt::ClientCredentials &credentials, const mqtt::Configuration &config) :
        mqtt::Client(broker, credentials, config)
    {
        constructed = true;
    }

private:
    void on_connected(esp_mqtt_event_handle_t const event) override
    {
        CHECK(constructed);
        connected_called = true;
    }

    void on_data(esp_mqtt_event_handle_t const event) override
    {
        CHECK(constructed);
    }
};
} // namespace

TEST_CASE("Client does not auto-start and can dispatch events after construction", "[esp_mqtt_cxx]")
{
    mqtt::BrokerConfiguration broker{
        .address = mqtt::URI{std::string{"mqtt://example.com"}},
        .security = mqtt::Insecure{}
    };
    mqtt::ClientCredentials credentials{};
    mqtt::Configuration config{};

    TestClient client{broker, credentials, config};

    REQUIRE(client.is_started() == false);

    esp_mqtt_event_t event{};
    event.event_id = MQTT_EVENT_CONNECTED;

    client.dispatch_event_for_test(static_cast<int32_t>(MQTT_EVENT_CONNECTED), &event);

    CHECK(client.connected_called);
}

extern "C" void app_main(void)
{
    Catch::Session session;

    int failures = session.run();
    if (failures > 0) {
        printf("TEST FAILED! number of failures=%d\n", failures);
        exit(1);
    }
    printf("Test passed!\n");
    exit(0);
}
