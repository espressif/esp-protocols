/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define EPPP_DEFAULT_SERVER_IP() ESP_IP4TOADDR(192, 168, 11, 1)
#define EPPP_DEFAULT_CLIENT_IP() ESP_IP4TOADDR(192, 168, 11, 2)

#define EPPP_DEFAULT_CONFIG(our_ip, their_ip) { \
    .uart = {   \
            .port = UART_NUM_1, \
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

typedef struct eppp_config_t {
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
