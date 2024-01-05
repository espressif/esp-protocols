/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define EPPP_DEFAULT_SERVER_IP() ESP_IP4TOADDR(192, 168, 11, 1)
#define EPPP_DEFAULT_CLIENT_IP() ESP_IP4TOADDR(192, 168, 11, 2)

#define EPPP_DEFAULT_CONFIG(our_ip, their_ip) { \
    .transport = EPPP_TRANSPORT_UART,           \
    .spi = {                                    \
            .host = 1,                          \
            .mosi = 11,                         \
            .miso = 13,                         \
            .sclk = 12,                         \
            .cs = 10,                           \
            .intr = 2,                          \
            .freq = 20*1000*1000,               \
    },                                          \
    .uart = {   \
            .port = 1,          \
            .baud = 921600,     \
            .tx_io = 25,        \
            .rx_io = 26,        \
            .queue_size = 16,   \
            .rx_buffer_size = 1024, \
    },  \
    . task = {                  \
            .run_task = true,   \
            .stack_size = 4096, \
            .priority = 18,     \
    },  \
    . ppp = {   \
            .our_ip4_addr = our_ip,     \
            .their_ip4_addr = their_ip, \
    }   \
}

#define EPPP_DEFAULT_SERVER_CONFIG() EPPP_DEFAULT_CONFIG(EPPP_DEFAULT_SERVER_IP(), EPPP_DEFAULT_CLIENT_IP())
#define EPPP_DEFAULT_CLIENT_CONFIG() EPPP_DEFAULT_CONFIG(EPPP_DEFAULT_CLIENT_IP(), EPPP_DEFAULT_SERVER_IP())

typedef enum eppp_type {
    EPPP_SERVER,
    EPPP_CLIENT,
} eppp_type_t;

typedef enum eppp_transport {
    EPPP_TRANSPORT_UART,
    EPPP_TRANSPORT_SPI,
} eppp_transport_t;


typedef struct eppp_config_t {
    eppp_transport_t transport;

    struct eppp_config_spi_s {
        int host;
        int mosi;
        int miso;
        int sclk;
        int cs;
        int intr;
        int freq;
    } spi;

    struct eppp_config_uart_s {
        int port;
        int baud;
        int tx_io;
        int rx_io;
        int queue_size;
        int rx_buffer_size;
    } uart;

    struct eppp_config_task_s {
        bool run_task;
        int stack_size;
        int priority;
    } task;

    struct eppp_config_pppos_s {
        uint32_t our_ip4_addr;
        uint32_t their_ip4_addr;
    } ppp;

} eppp_config_t;

esp_netif_t *eppp_connect(eppp_config_t *config);

esp_netif_t *eppp_listen(eppp_config_t *config);

void eppp_close(esp_netif_t *netif);

esp_netif_t *eppp_init(enum eppp_type role, eppp_config_t *config);

void eppp_deinit(esp_netif_t *netif);

esp_netif_t *eppp_open(enum eppp_type role, eppp_config_t *config, TickType_t connect_timeout);

esp_err_t eppp_netif_stop(esp_netif_t *netif, TickType_t stop_timeout);

esp_err_t eppp_netif_start(esp_netif_t *netif);

esp_err_t eppp_perform(esp_netif_t *netif);
