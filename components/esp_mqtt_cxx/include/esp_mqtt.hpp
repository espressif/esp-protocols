/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string_view>
#ifndef __cpp_exceptions
#error MQTT class can only be used when __cpp_exceptions is enabled. Enable CONFIG_COMPILER_CXX_EXCEPTIONS in Kconfig
#endif

#include <optional>
#include <variant>
#include <utility>
#include <memory>
#include <string>
#include "esp_exception.hpp"
#include "esp_mqtt_client_config.hpp"
#include "mqtt_client.h"

namespace idf::mqtt {

constexpr auto *TAG = "mqtt_client_cpp";

struct MQTTException : ESPException {
    using ESPException::ESPException;
};

/**
 * @brief QoS for publish and subscribe
 *
 * Sets the QoS as:
 * AtMostOnce  : Best effort delivery of messages. Message loss can occur.
 * AtLeastOnce : Guaranteed delivery of messages. Duplicates can occur.
 * ExactlyOnce : Guaranteed delivery of messages exactly once.
 *
 * @note
 *  When subscribing to a topic the QoS means the maximum QoS that should be sent to
 *  client on this topic
 */
enum class QoS { AtMostOnce = 0, AtLeastOnce = 1, ExactlyOnce = 2 };

/**
 * @brief Sets if a message must be retained.
 *
 * Retained messages are delivered to future subscribers that match the topic name.
 *
 */
enum class Retain : bool { NotRetained = false, Retained = true };


/**
 * @brief Message class template to publish.
 *
 */
template <typename T> struct Message {
    T data; /*!< Data for publish. Should be a contiguous type*/
    QoS qos = QoS::AtLeastOnce; /*!< QoS for the message*/
    Retain retain = Retain::NotRetained; /*!< Retention mark for the message.*/
};

/**
 * @brief Message type that holds std::string for data
 *
 */
using StringMessage = Message<std::string>;

[[nodiscard]] bool filter_is_valid(std::string::const_iterator first, std::string::const_iterator last);

/**
 * @brief Filter for mqtt topic subscription.
 *
 * Topic filter.
 *
 */
class Filter {
public:

    /**
     * @brief Constructs the topic filter from the user filter
     * @throws std::domain_error if the filter is invalid.
     *
     * @param user_filter Filter to be used.
     */
    explicit Filter(std::string user_filter);


    /**
     * @brief Get the filter string used.
     *
     * @return Reference to the topic filter.
     */
    const std::string &get();

    /**
     * @brief Checks the filter string against a topic name.
     *
     * @param topic_begin Iterator to the beginning of the sequence.
     * @param topic_end Iterator to the end of the sequence.
     *
     * @return true if the topic name match the filter
     */
    [[nodiscard]] bool match(std::string::const_iterator topic_begin, std::string::const_iterator topic_end) const noexcept;

    /**
     * @brief Checks the filter string against a topic name.
     *
     * @param topic topic name
     *
     * @return true if the topic name match the filter
     */
    [[nodiscard]] bool match(const std::string &topic) const noexcept;

    /**
     * @brief Checks the filter string against a topic name.
     *
     * @param begin Char array with topic name.
     * @param size  Size of given topic name.
     *
     * @return true if the topic name match the filter
     */
    [[nodiscard]] bool match(char *begin, int size) const noexcept;


private:

    /**
     * @brief Advance the topic to the next level.
     *
     * An mqtt topic ends with a /. This function is used to iterate in topic levels.
     *
     * @return Iterator to the start of the topic.
     */
    [[nodiscard]] std::string::const_iterator advance(std::string::const_iterator begin, std::string::const_iterator end) const;
    std::string filter;
};

/**
 * @brief Message identifier to track delivery.
 *
 */
enum class MessageID : int {};

/**
 * @brief Base class for MQTT client
 *
 * Should be inherited to provide event handlers.
 */
class Client {
public:
    /**
     * @brief Constructor of the client
     *
     * @param broker Configuration for broker connection
     * @param credentials client credentials to be presented to the broker
     * @param config Mqtt client configuration
     */
    Client(const BrokerConfiguration &broker, const ClientCredentials &credentials, const Configuration &config);

    /**
     * @brief Constructs Client using the same configuration used for
     * `esp_mqtt_client`
     * @param config config struct to `esp_mqtt_client`
     */
    Client(const esp_mqtt_client_config_t &config);

    /**
     * @brief Start the underlying esp-mqtt client
     *
     * Must be called after the derived class has finished constructing to avoid
     * events being dispatched to partially constructed objects.
     */
    void start();

    /**
     * @brief Check whether start() has been called
     */
    [[nodiscard]] bool is_started() const noexcept;

    /**
     * @brief Subscribe to topic
     *
     * @param topic_filter MQTT topic filter
     * @param qos QoS subscription, defaulted as QoS::AtLeastOnce
     *
     * @return Optional MessageID. In case of failure std::nullopt is returned.
     */
    std::optional<MessageID> subscribe(const std::string &topic_filter, QoS qos = QoS::AtLeastOnce);

    /**
     * @brief publish message to topic
     *
     * @tparam Container Type for data container. Must be a contiguous memory.
     * @param topic Topic name
     * @param message Message struct containing data, qos and retain
     * configuration.
     *
     * @return Optional MessageID. In case of failure std::nullopt is returned.
     */
    template <class Container>
    std::optional<MessageID> publish(const std::string &topic, const Message<Container> &message)
    {
        return publish(topic, std::begin(message.data), std::end(message.data), message.qos, message.retain);
    }

    /**
     * @brief publish message to topic
     *
     * @tparam InputIt Input data iterator type.
     * @param topic Topic name
     * @param first, last Iterator pair of data to publish
     * @param qos Set qos message
     * @param retain Set if message should be retained
     *
     * @return Optional MessageID. In case of failure std::nullopt is returned.
     */
    template <class InputIt>
    std::optional<MessageID> publish(const std::string &topic, InputIt first, InputIt last, QoS qos = QoS::AtLeastOnce, Retain retain = Retain::NotRetained)
    {
        auto size = std::distance(first, last);
        auto res =  esp_mqtt_client_publish(handler.get(), topic.c_str(), &(*first), size, static_cast<int>(qos),
                                            static_cast<int>(retain));
        if (res < 0) {
            return std::nullopt;
        }
        return MessageID{res};
    }

    virtual ~Client() = default;

    /**
    * @brief Test helper to dispatch events without a broker connection
    *
    * Intended for unit tests; forwards directly to the internal event handler.
    */
    void dispatch_event_for_test(int32_t event_id, esp_mqtt_event_t *event);


protected:
    /**
     * @brief Helper type to be used as custom deleter for std::unique_ptr.
     */
    struct MqttClientDeleter {
        void operator()(esp_mqtt_client *client_handler)
        {
            esp_mqtt_client_destroy(client_handler);
        }
    };

    /**
     * @brief Type of the handler for the underlying mqtt_client handler.
     * It uses std::unique_ptr for lifetime management
     */
    using ClientHandler = std::unique_ptr<esp_mqtt_client, MqttClientDeleter>;
    /**
     * @brief esp_mqtt_client handler
     *
     */
    ClientHandler handler;

    /**
    * @brief Called if there is an error event
    *
    * @param event mqtt event data
    */
    virtual void on_error(const esp_mqtt_event_handle_t event);
    /**
    * @brief Called if there is an disconnection event
    *
    * @param event mqtt event data
    */
    virtual void on_disconnected(const esp_mqtt_event_handle_t event);
    /**
    * @brief Called if there is an subscribed event
    *
    * @param event mqtt event data
    */
    virtual void on_subscribed(const esp_mqtt_event_handle_t event);
    /**
    * @brief Called if there is an unsubscribed event
    *
    * @param event mqtt event data
    */
    virtual void on_unsubscribed(const esp_mqtt_event_handle_t event);
    /**
    * @brief Called if there is an published event
    *
    * @param event mqtt event data
    */
    virtual void on_published(const esp_mqtt_event_handle_t event);
    /**
    * @brief Called if there is an before connect event
    *
    * @param event mqtt event data
    */
    virtual void on_before_connect(const esp_mqtt_event_handle_t event);
    /**
    * @brief Called if there is an connected event
    *
    * @param event mqtt event data
    *
    */
    virtual void on_connected(const esp_mqtt_event_handle_t event) = 0;
    /**
    * @brief Called if there is an data event
    *
    * @param event mqtt event data
    *
    */
    virtual void on_data(const esp_mqtt_event_handle_t event) = 0;
private:
    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                   void *event_data) noexcept;
    void init(const esp_mqtt_client_config_t &config);
    bool started{false};
};
} // namespace idf::mqtt
