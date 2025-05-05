/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_netif_types.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_crc.h"

#if 0
struct eppp_handle {
    esp_netif_driver_base_t base;
    eppp_type_t role;
    bool stop;
    bool exited;
    bool netif_stop;
};
#endif

struct packet {
    size_t len;
    uint8_t *data;
};

#if 1

#if CONFIG_EPPP_LINK_DEVICE_SPI
#define MAX_PAYLOAD 1500
#define MIN_TRIGGER_US 20
#define SPI_HEADER_MAGIC 0x1234



struct header {
    uint16_t magic;
    uint16_t size;
    uint16_t next_size;
    uint16_t check;
} __attribute__((packed));

enum blocked_status {
    NONE,
    MASTER_BLOCKED,
    MASTER_WANTS_READ,
    SLAVE_BLOCKED,
    SLAVE_WANTS_WRITE,
};

#endif // CONFIG_EPPP_LINK_DEVICE_SPI


struct eppp_handle {
    esp_netif_driver_base_t base;
#if CONFIG_EPPP_LINK_DEVICE_SPI
    bool is_master;
    QueueHandle_t out_queue;
    QueueHandle_t ready_semaphore;
    spi_device_handle_t spi_device;
    spi_host_device_t spi_host;
    int gpio_intr;
    uint16_t next_size;
    uint16_t transaction_size;
    struct packet outbound;
    enum blocked_status blocked;
    uint32_t slave_last_edge;
    esp_timer_handle_t timer;
#elif CONFIG_EPPP_LINK_DEVICE_UART
    QueueHandle_t uart_event_queue;
    uart_port_t uart_port;
#endif
    esp_netif_t *netif;
    eppp_type_t role;
    bool stop;
    bool exited;
    bool netif_stop;
};
#endif



//esp_err_t eppp_transport_init(eppp_config_t *config, esp_netif_t *esp_netif);
//
//esp_err_t eppp_transport_tx(void *h, void *buffer, size_t len);
//
//esp_err_t eppp_transport_rx(esp_netif_t *netif);
//
//void eppp_transport_deinit(void);
