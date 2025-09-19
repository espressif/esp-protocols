/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * CSLIP Client Example (initial, pass-through SLIP)
*/
#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "cslip_modem.h"

static const char *TAG = "cslip-example";

#define STACK_SIZE (10 * 1024)
#define PRIORITY   10

static void udp_rx_tx_task(void *arg)
{
    char addr_str[128];
    uint8_t rx_buff[1024];

    int sock = (int)arg;

    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);


    ESP_LOGI(TAG, "Starting UDP echo task");

    while (1) {
        int len = recvfrom(sock, rx_buff, sizeof(rx_buff) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        } else if (source_addr.ss_family == PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }

        rx_buff[len] = 0;
        ESP_LOGI(TAG, "Received '%s' from '%s'", rx_buff, addr_str);

        int err = sendto(sock, rx_buff, len, 0, (struct sockaddr *)&source_addr, socklen);
        if (err < 0) {
            ESP_LOGE(TAG, "sendto failed: errno %d", errno);
            break;
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t udp_rx_tx_start(void)
{
    struct sockaddr_in6 dest_addr;
#if CONFIG_EXAMPLE_IPV4
    sa_family_t family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(CONFIG_EXAMPLE_UDP_PORT);
    ip_protocol = IPPROTO_IP;
#else
    sa_family_t family = AF_INET6;
    int ip_protocol = IPPROTO_IPV6;
    bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
    dest_addr.sin6_family = family;
    dest_addr.sin6_port = htons(CONFIG_EXAMPLE_UDP_PORT);
#endif

    int sock = socket(family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if !CONFIG_EXAMPLE_IPV4
    setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", CONFIG_EXAMPLE_UDP_PORT);

    xTaskCreate(udp_rx_tx_task, "udp_rx_tx", STACK_SIZE, (void *)sock, PRIORITY, NULL);

    return ESP_OK;
}

#if CONFIG_EXAMPLE_IPV4
static const esp_netif_ip_info_t s_cslip_ip4 = {
    .ip = { .addr = ESP_IP4TOADDR(10, 0, 0, 2) },
};
#endif

// Initialise the CSLIP-like interface (currently pass-through SLIP)
esp_netif_t *cslip_if_init(void)
{
    ESP_LOGI(TAG, "Initialising CSLIP interface (pass-through)");

    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_CSLIP();
#if CONFIG_EXAMPLE_IPV4
    base_cfg.ip_info = &s_cslip_ip4;
#endif
    esp_netif_config_t cfg = { .base = &base_cfg, .driver = NULL, .stack = netstack_default_cslip };

    esp_netif_t *cslip_netif = esp_netif_new(&cfg);

    esp_ip6_addr_t local_addr;        /* Local IP6 address */
    IP6_ADDR(&local_addr,
             lwip_htonl(0xfd0000),
             lwip_htonl(0x00000000),
             lwip_htonl(0x00000000),
             lwip_htonl(0x00000001)
            );

    ESP_LOGI(TAG, "Initialising CSLIP modem");

    cslip_modem_config_t modem_cfg = {
        .uart_dev = UART_NUM_1,
        .uart_tx_pin = CONFIG_EXAMPLE_UART_TX_PIN,
        .uart_rx_pin = CONFIG_EXAMPLE_UART_RX_PIN,
        .uart_baud = CONFIG_EXAMPLE_UART_BAUD,
        .rx_buffer_len = 1024,
        .rx_filter = NULL, // optional: same behavior as SLIP
        .ipv6_addr = &local_addr,
        .cslip = {
            .enable = true,
            .vj_slots = 16,
            .slotid_compression = true,
            .safe_mode = true,
        },
    };

    void *cslip_modem = cslip_modem_create(cslip_netif, &modem_cfg);
    assert(cslip_modem);
    ESP_ERROR_CHECK(esp_netif_attach(cslip_netif, cslip_modem));

    ESP_LOGI(TAG, "CSLIP init complete");

    return cslip_netif;
}

void app_main(void)
{
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Setup cslip interface (pass-through)
    esp_netif_t *esp_netif = cslip_if_init();
    assert(esp_netif);

    // Start the UDP user application
    udp_rx_tx_start();
}
