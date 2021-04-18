/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include <iostream>

#define BROKER_URL "mqtt://mqtt.eclipseprojects.io"

using namespace esp_modem;

static const char *TAG = "cmux_example";

static EventGroupHandle_t event_group = nullptr;
static const int CONNECT_BIT = BIT0;
static const int STOP_BIT = BIT1;
static const int GOT_DATA_BIT = BIT2;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/esp-pppos", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/esp-pppos", "esp32-pppos", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        xEventGroupSetBits(event_group, GOT_DATA_BIT);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
        break;
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = (esp_netif_t *)event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}


static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}



extern "C" void app_main(void)
{
    /* Init and register system/core components */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    event_group = xEventGroupCreate();

    /* Configure the DTE */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

    /* Configure the DCE */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);

    /* Configure the PPP netif */
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

    auto uart_dte = create_uart_dte(&dte_config);

    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    #if CONFIG_EXAMPLE_MODEM_DEVICE_BG96 == 1
    auto dce = create_BG96_dce(&dce_config, uart_dte, esp_netif);
    #elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM800 == 1
    auto dce = create_SIM800_dce(&dce_config, uart_dte, esp_netif);
    #elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600 == 1
    auto dce = create_SIM7600_dce(&dce_config, uart_dte, esp_netif);
    #else
    #error "Unsupported device"
    #endif

    dce->set_command_mode();

    std::string str;
    dce->get_module_name(str);
    std::cout << "Module name:" << str << std::endl;
    bool pin_ok = true;
    if (dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
        throw_if_false(dce->set_pin(CONFIG_EXAMPLE_SIM_PIN) == command_result::OK, "Cannot set PIN!");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    dce->set_mode(esp_modem::modem_mode::CMUX_MODE);
    dce->get_imsi(str);
    std::cout << "Modem IMSI number:" << str << "|" << std::endl;

    dce->set_data();

    // MQTT connection
    xEventGroupWaitBits(event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    esp_mqtt_client_config_t mqtt_config = { };
    mqtt_config.uri = BROKER_URL;
    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
    esp_mqtt_client_start(mqtt_client);

    EventBits_t got_data = 0;
    while (got_data == 0) { // reading IMSI number until we get published data from MQTT
        dce->get_imsi(str);
        std::cout << "Modem IMSI number:" << str << "|" << std::endl;
        got_data = xEventGroupWaitBits(event_group, GOT_DATA_BIT, pdTRUE, pdTRUE, pdMS_TO_TICKS(500));
    }
    esp_mqtt_client_destroy(mqtt_client);
    dce->get_imsi(str);
    std::cout << "Modem IMSI number:" << str << "|" << std::endl;
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Example finished");
}
