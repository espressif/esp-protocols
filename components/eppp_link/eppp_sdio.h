/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#define MAX_SDIO_PAYLOAD 1500
#define SDIO_ALIGN(size) (((size) + 3U) & ~(3U))
#define SDIO_PAYLOAD SDIO_ALIGN(MAX_SDIO_PAYLOAD)
#define SDIO_PACKET_SIZE SDIO_ALIGN(MAX_SDIO_PAYLOAD + 4)
#define PPP_SOF 0x7E


// Interrupts and registers
#define SLAVE_INTR      0
#define SLAVE_REG_REQ   0

// Requests from host to slave
#define REQ_RESET 1
#define REQ_INIT  2

struct header {
    uint8_t magic;
    uint8_t channel;
    uint16_t size;
} __attribute__((packed));

esp_err_t eppp_sdio_transmit_channel(esp_netif_t *netif, int channel, void *buffer, size_t len);
