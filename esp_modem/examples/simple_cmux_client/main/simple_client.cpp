/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <cstring>
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "esp_event_cxx.hpp"
#include "simple_mqtt_client.hpp"
#include "esp_vfs_dev.h"        // For optional VFS support
#include "esp_https_ota.h"      // For potential OTA configuration


#define BROKER_URL "mqtt://mqtt.eclipseprojects.io"


using namespace esp_modem;
using namespace idf::event;


static const char *TAG = "cmux_example";


extern "C" void app_main(void)
{
    /* Init and register system/core components */
    auto loop = std::make_shared<ESPEventLoop>();
    ESP_ERROR_CHECK(esp_netif_init());

    /* Configure and create the DTE */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
#if CONFIG_EXAMPLE_USE_VFS_TERM == 1
    /* The VFS terminal is just a demonstration of using an abstract file descriptor
     * which implements non-block reads, writes and selects to communicate with esp-modem.
     * This configuration uses the same UART driver as the terminal created by `create_uart_dte()`,
     * so doesn't give any practical benefit besides the FD use demonstration and a placeholder
     * to use FD terminal for other devices
     */
    dte_config.vfs_config.dev_name = "/dev/uart/1";
    dte_config.vfs_config.resource = ESP_MODEM_VFS_IS_UART;
    dte_config.uart_config.event_queue_size = 0;
    auto dte = create_vfs_dte(&dte_config);
    esp_vfs_dev_uart_use_driver(dte_config.uart_config.port_num);
#else
    auto dte = create_uart_dte(&dte_config);
#endif // CONFIG_EXAMPLE_USE_VFS_TERM
    assert(dte);

    /* Configure the DCE */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);

    /* Configure the PPP netif */
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

    /* Create the PPP and DCE objects */

    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

#if CONFIG_EXAMPLE_MODEM_DEVICE_BG96 == 1
    std::unique_ptr<DCE> dce = create_BG96_dce(&dce_config, dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM800 == 1
    std::unique_ptr<DCE> dce = create_SIM800_dce(&dce_config, uart_dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600 == 1
    std::unique_ptr<DCE> dce = create_SIM7600_dce(&dce_config, uart_dte, esp_netif);
#else
#error "Unsupported device"
#endif
    assert(dce);

    /* Setup basic operation mode for the DCE (pin if used, CMUX mode) */
#if CONFIG_EXAMPLE_NEED_SIM_PIN == 1
    bool pin_ok = true;
    if (dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
        throw_if_false(dce->set_pin(CONFIG_EXAMPLE_SIM_PIN) == command_result::OK, "Cannot set PIN!");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Need to wait for some time after unlocking the SIM
    }
#endif

    if (dce->set_mode(esp_modem::modem_mode::CMUX_MODE) && dce->set_mode(esp_modem::modem_mode::DATA_MODE)) {
        std::cout << "Modem has correctly entered multiplexed command/data mode" << std::endl;
    } else {
        ESP_LOGE(TAG, "Failed to configure desired mode... exiting");
        return;
    }

    /* Read some data from the modem */
    std::string str;
    while (dce->get_operator_name(str) != esp_modem::command_result::OK) {
        // Getting operator name could fail... retry after 500 ms
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    std::cout << "Operator name:" << str << std::endl;

    /* Try to connect to the network and publish an mqtt topic */
    ESPEventHandlerSync event_handler(loop);
    event_handler.listen_to(ESPEvent(IP_EVENT, ESPEventID(ESP_EVENT_ANY_ID)));
    auto result = event_handler.wait_event_for(std::chrono::milliseconds(60000));
    if (result.timeout) {
        ESP_LOGE(TAG, "Cannot get IP within specified timeout... exiting");
        return;
    } else if (result.event.id == ESPEventID(IP_EVENT_PPP_GOT_IP)) {
        auto *event = (ip_event_got_ip_t *)result.ev_data;
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        std::cout << "Got IP address" << std::endl;

        /* When connected to network, subscribe and publish some MQTT data */
        MqttClient mqtt(BROKER_URL);
        event_handler.listen_to(MqttClient::get_event(MqttClient::Event::CONNECT));
        event_handler.listen_to(MqttClient::get_event(MqttClient::Event::DATA));

        auto reg = loop->register_event(MqttClient::get_event(MqttClient::Event::DATA),
                                        [&mqtt](const ESPEvent &event, void *data) {
                                            std::cout << " TOPIC:" << mqtt.get_topic(data) << std::endl;
                                            std::cout << " DATA:" << mqtt.get_data(data) << std::endl;
                                        });
        mqtt.connect();
        while (true) {
            result = event_handler.wait_event_for(std::chrono::milliseconds(60000));
            if (result.event == MqttClient::get_event(MqttClient::Event::CONNECT)) {
                mqtt.subscribe("/topic/esp-modem");
                mqtt.publish("/topic/esp-modem", "Hello modem");
                continue;
            } else if (result.event == MqttClient::get_event(MqttClient::Event::DATA)) {
                std::cout << "Data received" << std::endl;
                break; /* Continue with CMUX example after getting data from MQTT */
            } else {
                break;
            }
        }

    } else if (result.event.id == ESPEventID(IP_EVENT_PPP_LOST_IP)) {
        ESP_LOGE(TAG, "PPP client has lost connection... exiting");
        return;
    }

    /* Again reading some data from the modem */
    if (dce->get_imsi(str) == esp_modem::command_result::OK) {
        std::cout << "Modem IMSI number:" << str << std::endl;
    }

#if CONFIG_EXAMPLE_PERFORM_OTA == 1
    esp_http_client_config_t config = { };
    config.skip_cert_common_name_check = true;
    config.url = CONFIG_EXAMPLE_PERFORM_OTA_URI;

    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
        return;
    }
#endif // CONFIG_EXAMPLE_PERFORM_OTA
}
