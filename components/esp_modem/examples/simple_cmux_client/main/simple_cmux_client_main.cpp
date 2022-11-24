/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
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
#include "vfs_resource/vfs_create.hpp"
#include "SIM7070_gnss.hpp"

#if defined(CONFIG_EXAMPLE_FLOW_CONTROL_NONE)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_SW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_HW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif

#define BROKER_URL CONFIG_BROKER_URI


using namespace esp_modem;
using namespace idf::event;


static const char *TAG = "cmux_example";



/**
 *
 * @param dce
 * very relevant here to have "& dce" instead of "dce"
 * see https://stackoverflow.com/questions/30905487/how-can-i-pass-stdunique-ptr-into-a-function
 *
 * @param m
 */
void set_mode_and_report(std::unique_ptr<DCE_gnss> &dce, modem_mode m)
{
    std::string str;
    switch (m) {
    case modem_mode::UNDEF:               str = "UNDEF";               break;
    case modem_mode::COMMAND_MODE:        str = "COMMAND_MODE";        break;
    case modem_mode::DATA_MODE:           str = "DATA_MODE";           break;
    case modem_mode::CMUX_MODE:           str = "CMUX_MODE";           break;
    case modem_mode::CMUX_MANUAL_MODE:    str = "CMUX_MANUAL_MODE";    break;
    case modem_mode::CMUX_MANUAL_EXIT:    str = "CMUX_MANUAL_EXIT";    break;
    case modem_mode::CMUX_MANUAL_DATA:    str = "CMUX_MANUAL_DATA";    break;
    case modem_mode::CMUX_MANUAL_COMMAND: str = "CMUX_MANUAL_COMMAND"; break;
    case modem_mode::CMUX_MANUAL_SWAP:    str = "CMUX_MANUAL_SWAP";    break;
    }

    if (dce->set_mode(m)) {
        std::cout << "Modem has correctly entered mode " << str << std::endl;
    } else {
        ESP_LOGE(TAG, "Failed to configure desired mode... exiting");
        return;
    }
}


extern "C" void simple_cmux_client_main(void)
{
    /* Init and register system/core components */
    auto loop = std::make_shared<ESPEventLoop>();
    ESP_ERROR_CHECK(esp_netif_init());

    /* Configure and create the DTE */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;
    dte_config.uart_config.flow_control = EXAMPLE_FLOW_CONTROL;
#if CONFIG_EXAMPLE_USE_VFS_TERM == 1
    /* The VFS terminal is just a demonstration of using an abstract file descriptor
     * which implements non-block reads, writes and selects to communicate with esp-modem.
     * This configuration uses the same UART driver as the terminal created by `create_uart_dte()`,
     * so doesn't give any practical benefit besides the FD use demonstration and a placeholder
     * to use FD terminal for other devices
     */
    struct esp_modem_vfs_uart_creator uart_config = ESP_MODEM_VFS_DEFAULT_UART_CONFIG("/dev/uart/1");
    assert(vfs_create_uart(&uart_config, &dte_config.vfs_config) == true);

    auto dte = create_vfs_dte(&dte_config);
    esp_vfs_dev_uart_use_driver(uart_config.uart.port_num);
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
    auto dce = create_BG96_dce(&dce_config, dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM800 == 1
    auto dce = create_SIM800_dce(&dce_config, dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7000 == 1
    auto dce = create_SIM7000_dce(&dce_config, dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7070 == 1
    auto dce = create_SIM7070_dce(&dce_config, dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7070_GNSS == 1
    auto dce = create_SIM7070_GNSS_dce(&dce_config, dte, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600 == 1
    auto dce = create_SIM7600_dce(&dce_config, dte, esp_netif);
#else
#error "Unsupported device"
#endif
    assert(dce);

    if (dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {

        //now we want to go back to 2-Wire mode:
        dte->set_flow_control(ESP_MODEM_FLOW_CONTROL_NONE);


        for (int i = 0; i < 15; ++i) {
            if (command_result::OK != dce->sync()) {
                ESP_LOGW(TAG, "sync no Success after %i try", i);
            } else {
                ESP_LOGI(TAG, "sync Success after %i try", i);
                break; //exit the Loop.
            }
        }


        //now we want to go back to 4-Wire mode:
        dte->set_flow_control(ESP_MODEM_FLOW_CONTROL_HW);

        //set this mode also to the DCE.
        if (command_result::OK != dce->set_flow_control(2, 2)) {
            ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
            return;
        }
        ESP_LOGI(TAG, "set_flow_control OK");


        //sync
        for (int i = 0; i < 15; ++i) {
            if (command_result::OK != dce->sync()) {
                ESP_LOGE(TAG, "sync no Success after %i try", i);
            } else {
                ESP_LOGI(TAG, "sync Success after %i try", i);
                break; //exit the Loop.
            }
        }
    } else {
        ESP_LOGI(TAG, "not set_flow_control, because 2-wire mode active.");
    }

    /* Setup basic operation mode for the DCE (pin if used, CMUX mode) */
#if CONFIG_EXAMPLE_NEED_SIM_PIN == 1
    bool pin_ok = true;
    if (dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
        ESP_MODEM_THROW_IF_FALSE(dce->set_pin(CONFIG_EXAMPLE_SIM_PIN) == command_result::OK, "Cannot set PIN!");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Need to wait for some time after unlocking the SIM
    }
#endif

    set_mode_and_report(dce, esp_modem::modem_mode::CMUX_MANUAL_MODE);       /*!< Enter CMUX mode manually -- just creates two virtual terminals */
    set_mode_and_report(dce, esp_modem::modem_mode::CMUX_MANUAL_DATA);       // second Terminal to Data (Swap is implicit)




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



label_loop:



        /* When connected to network, subscribe and publish some MQTT data */
        MqttClient mqtt(BROKER_URL);
        event_handler.listen_to(MqttClient::get_event(MqttClient::Event::CONNECT));
        event_handler.listen_to(MqttClient::get_event(MqttClient::Event::DATA));

        auto reg = loop->register_event(MqttClient::get_event(MqttClient::Event::DATA),
        [&mqtt](const ESPEvent & event, void *data) {
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


        /* Again reading some data from the modem */
        if (dce->get_imsi(str) == esp_modem::command_result::OK) {
            std::cout << "Modem IMSI number:" << str << std::endl;
        }


        /* Again reading some data from the modem */
        switch (dce->get_gnss_information_sim70xx_once_req()) {
        case command_result::OK:             /*!< The command completed successfully */
            std::cout << "get_gnss_information_sim70xx_once OK" << std::endl;
            break;
        case command_result::FAIL:           /*!< The command explicitly failed */
            std::cout << "get_gnss_information_sim70xx_once fail" << std::endl;
            break;
        case command_result::TIMEOUT:        /*!< The device didn't respond in the specified timeline */
            std::cout << "get_gnss_information_sim70xx_once timeout" << std::endl;
            break;
        }
        vTaskDelay( 30500 / portTICK_PERIOD_MS );



        goto label_loop;

    } else if (result.event.id == ESPEventID(IP_EVENT_PPP_LOST_IP)) {
        ESP_LOGE(TAG, "PPP client has lost connection... exiting");
        return;
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
