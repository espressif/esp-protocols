/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_log.h"
#include "esp_console.h"
#include "mdns.h"


static const char *TAG = "mdns-stub";
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

typedef void (*callback_t)(const uint8_t *, size_t len);

static callback_t rust_callback = NULL;



void set_callback(callback_t callback)
{
    rust_callback = callback;
}

void set_callback2()
{
    ESP_LOGI(TAG, "set_callback2!");
}


esp_err_t _mdns_send_rx_action(mdns_rx_packet_t *packet)
{
    ESP_LOGI(TAG, "Received packet!");
    ESP_LOG_BUFFER_HEXDUMP(TAG, packet->pb->payload, packet->pb->tot_len, ESP_LOG_INFO);
    if (rust_callback) {
        rust_callback(packet->pb->payload, packet->pb->tot_len);
    }
    _mdns_packet_free(packet);
    return ESP_OK;
}

esp_netif_t *g_netif = NULL;


esp_netif_t *_mdns_get_esp_netif(mdns_if_t tcpip_if)
{
    if (g_netif == NULL) {
        const esp_netif_inherent_config_t base_cg = { .if_key = "WIFI_STA_DEF", .if_desc = CONFIG_TEST_NETIF_NAME };
        esp_netif_config_t cfg = { .base = &base_cg  };
        g_netif = esp_netif_new(&cfg);
    }
    return g_netif;
}
