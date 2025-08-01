/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define EPPP_DEFAULT_SERVER_IP() ESP_IP4TOADDR(192, 168, 11, 1)
#define EPPP_DEFAULT_CLIENT_IP() ESP_IP4TOADDR(192, 168, 11, 2)

#define EPPP_DEFAULT_SPI_CONFIG()               \
    .spi = {                                    \
            .host = 1,                          \
            .mosi = 11,                         \
            .miso = 13,                         \
            .sclk = 12,                         \
            .cs = 10,                           \
            .intr = 2,                          \
            .freq = 16*1000*1000,               \
            .input_delay_ns = 0,                \
            .cs_ena_pretrans = 0,               \
            .cs_ena_posttrans = 0,              \
    },                                          \

#define EPPP_DEFAULT_UART_CONFIG() \
    .uart = {   \
            .port = 1,          \
            .baud = 921600,     \
            .tx_io = 25,        \
            .rx_io = 26,        \
            .queue_size = 16,   \
            .rx_buffer_size = 1024, \
    },                          \

#define EPPP_DEFAULT_SDIO_CONFIG() \
    .sdio = {                                   \
            .width = 4,                         \
            .clk = 18,                          \
            .cmd = 19,                          \
            .d0 = 14,                           \
            .d1 = 15,                           \
            .d2 = 16,                           \
            .d3 = 17,           \
    },                          \

#define EPPP_DEFAULT_ETH_CONFIG()   \
    .ethernet = {                   \
        .mdc_io = 23,               \
        .mdio_io = 18,              \
        .phy_addr = 1,              \
        .rst_io= 5,                 \
    },                              \

#if CONFIG_EPPP_LINK_DEVICE_SPI
#define EPPP_DEFAULT_TRANSPORT_CONFIG() EPPP_DEFAULT_SPI_CONFIG()
#define EPPP_TRANSPORT_INIT(cfg)        eppp_spi_init(&cfg->spi)
#define EPPP_TRANSPORT_DEINIT(handle)   eppp_spi_deinit(handle)

#elif CONFIG_EPPP_LINK_DEVICE_UART
#define EPPP_DEFAULT_TRANSPORT_CONFIG() EPPP_DEFAULT_UART_CONFIG()
#define EPPP_TRANSPORT_INIT(cfg)        eppp_uart_init(&cfg->uart)
#define EPPP_TRANSPORT_DEINIT(handle)   eppp_uart_deinit(handle)

#elif CONFIG_EPPP_LINK_DEVICE_SDIO
#define EPPP_DEFAULT_TRANSPORT_CONFIG() EPPP_DEFAULT_SDIO_CONFIG()
#define EPPP_TRANSPORT_INIT(cfg)        eppp_sdio_init(&cfg->sdio)
#define EPPP_TRANSPORT_DEINIT(handle)   eppp_sdio_deinit(handle)

#elif CONFIG_EPPP_LINK_DEVICE_ETH
#define EPPP_DEFAULT_TRANSPORT_CONFIG() EPPP_DEFAULT_ETH_CONFIG()
#define EPPP_TRANSPORT_INIT(cfg)        eppp_eth_init(&cfg->ethernet)
#define EPPP_TRANSPORT_DEINIT(handle)   eppp_eth_deinit(handle)

#else
#error Unexpeted transport
#endif


#define EPPP_DEFAULT_CONFIG(our_ip, their_ip) { \
    .transport = EPPP_TRANSPORT_UART,           \
    EPPP_DEFAULT_TRANSPORT_CONFIG()             \
    . task = {                  \
            .run_task = true,   \
            .stack_size = 4096, \
            .priority = 8,      \
    },  \
    . ppp = {   \
            .our_ip4_addr = { .addr = our_ip },     \
            .their_ip4_addr = { .addr = their_ip }, \
            .netif_prio = 0,                        \
            .netif_description = NULL,              \
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
    EPPP_TRANSPORT_SDIO,
    EPPP_TRANSPORT_ETHERNET,
} eppp_transport_t;


typedef struct eppp_config_t {
    eppp_transport_t transport;
    struct eppp_config_spi_s {
        int host;
        bool is_master;
        int mosi;
        int miso;
        int sclk;
        int cs;
        int intr;
        int freq;
        int input_delay_ns;
        int cs_ena_pretrans;
        int cs_ena_posttrans;
    } spi;

    struct eppp_config_uart_s {
        int port;
        int baud;
        int tx_io;
        int rx_io;
        int queue_size;
        int rx_buffer_size;
    } uart;

    struct eppp_config_sdio_s {
        bool is_host;
        int width;
        int clk;
        int cmd;
        int d0;
        int d1;
        int d2;
        int d3;
    } sdio;

    struct eppp_config_ethernet_s {
        int mdc_io;
        int mdio_io;
        int phy_addr;
        int rst_io;
    } ethernet;

    struct eppp_config_task_s {
        bool run_task;
        int stack_size;
        int priority;
    } task;

    struct eppp_config_pppos_s {
        esp_ip4_addr_t our_ip4_addr;
        esp_ip4_addr_t their_ip4_addr;
        int netif_prio;
        const char *netif_description;
    } ppp;

} eppp_config_t;

typedef struct eppp_handle* eppp_transport_handle_t;

esp_netif_t *eppp_connect(eppp_config_t *config);

esp_netif_t *eppp_listen(eppp_config_t *config);

void eppp_close(esp_netif_t *netif);

/**
 * @brief Initialize the EPPP link layer
 * @param role
 * @param config
 * @return
 */
esp_netif_t *eppp_init(eppp_type_t role, eppp_config_t *config);

void eppp_deinit(esp_netif_t *netif);

esp_netif_t *eppp_open(eppp_type_t role, eppp_config_t *config, int connect_timeout_ms);

esp_netif_t *eppp_netif_init(eppp_type_t role, eppp_transport_handle_t h, eppp_config_t *eppp_config);

esp_err_t eppp_netif_stop(esp_netif_t *netif, int stop_timeout_ms);

esp_err_t eppp_netif_start(esp_netif_t *netif);

void eppp_netif_deinit(esp_netif_t *netif);

/**
 * @brief Performs EPPP link task operation (Used for task-less implementation)
 * @param netif
 * @return
 *  - ESP_OK on success, ESP_FAIL on failure: the operation shall continue
 *  - ESP_ERR_TIMEOUT indicates that the operation was requested to stop
 */
esp_err_t eppp_perform(esp_netif_t *netif);

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
typedef esp_err_t (*eppp_channel_fn_t)(esp_netif_t *netif, int nr, void *buffer, size_t len);

esp_err_t eppp_add_channels(esp_netif_t *netif, eppp_channel_fn_t *tx, const eppp_channel_fn_t rx, void* context);

void* eppp_get_context(esp_netif_t *netif);
#endif // CONFIG_EPPP_LINK_CHANNELS_SUPPORT

#ifdef __cplusplus
}
#endif
