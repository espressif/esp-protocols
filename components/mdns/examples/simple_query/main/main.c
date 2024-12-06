/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "mdns.h"

static const char *TAG = "mdns-test";

static esp_netif_t *s_netif;

static void mdns_test_app(esp_netif_t *interface);

#ifndef CONFIG_IDF_TARGET_LINUX
#include "protocol_examples_common.h"
#include "esp_event.h"
#include "nvs_flash.h"

/**
 * @brief This is an entry point for the real target device,
 * need to init few components and connect to a network interface
 */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    mdns_test_app(EXAMPLE_INTERFACE);

    ESP_ERROR_CHECK(example_disconnect());
}
#else

/**
 * @brief This is an entry point for the linux target (simulator on host)
 * need to create a dummy WiFi station and use it as mdns network interface
 */
int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const esp_netif_inherent_config_t base_cg = { .if_key = "WIFI_STA_DEF", .if_desc = CONFIG_TEST_NETIF_NAME };
    esp_netif_config_t cfg = { .base = &base_cg  };
    s_netif = esp_netif_new(&cfg);

    mdns_test_app(s_netif);

    esp_netif_destroy(s_netif);
    return 0;
}
#endif

typedef size_t mdns_if_t;

typedef struct {
    mdns_if_t tcpip_if;
    mdns_ip_protocol_t ip_protocol;
    struct pbuf *pb;
    esp_ip_addr_t src;
    esp_ip_addr_t dest;
    uint16_t src_port;
    uint8_t multicast;
} mdns_rx_packet_t;

struct pbuf  {
    struct pbuf *next;
    void *payload;
    size_t tot_len;
    size_t len;
};

esp_err_t _mdns_pcb_init(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
size_t _mdns_udp_pcb_write(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol, const esp_ip_addr_t *ip, uint16_t port, uint8_t *data, size_t len);
esp_err_t _mdns_pcb_deinit(mdns_if_t tcpip_if, mdns_ip_protocol_t ip_protocol);
void _mdns_packet_free(mdns_rx_packet_t *packet);


static void mdns_test_app(esp_netif_t *interface)
{
    uint8_t query_packet[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x64, 0x61, 0x76, 0x69, 0x64, 0x2d, 0x77, 0x6f, 0x72, 0x6b, 0x05, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, 0x00, 0x01, 0x00, 0x01};
    esp_ip_addr_t ip = ESP_IP4ADDR_INIT(224, 0, 0, 251);

//    ESP_ERROR_CHECK(mdns_init());
//    ESP_ERROR_CHECK(mdns_register_netif(interface));
//    ESP_ERROR_CHECK(mdns_netif_action(interface, MDNS_EVENT_ENABLE_IP4));
    esp_err_t err = _mdns_pcb_init(0, MDNS_IP_PROTOCOL_V4);
    ESP_LOGI(TAG, "err = %d", err);
    size_t len = _mdns_udp_pcb_write(0, MDNS_IP_PROTOCOL_V4, &ip, 5353, query_packet, sizeof(query_packet));
    ESP_LOGI(TAG, "len = %d", (int)len);
//    query_mdns_host("david-work");
    vTaskDelay(pdMS_TO_TICKS(1000));
//    mdns_free();
    _mdns_pcb_deinit(0, MDNS_IP_PROTOCOL_V4);
    ESP_LOGI(TAG, "Exit");
}
