/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "net_connect.h"
#include "esp_err.h"
#include "driver/uart_vfs.h"
#include "driver/uart.h"
#include "sdkconfig.h"

/**
 * @brief Configure stdin and stdout to use blocking I/O via UART
 *
 * This function initializes the UART driver and configures the VFS layer to use
 * it for console I/O operations. It sets up stdin/stdout to work with standard
 * C/C++ I/O functions (e.g., scanf, printf, std::cin, std::cout).
 *
 * The function is idempotent - if the UART driver is already installed, it
 * returns immediately without re-initializing.
 *
 * Configuration details:
 * - Sets stdin to unbuffered mode for immediate input processing
 * - Installs interrupt-driven UART driver with 256-byte RX buffer
 * - Configures line endings: CR for received data, CRLF for transmitted data
 *
 * @return ESP_OK on success
 */
esp_err_t net_configure_stdin_stdout(void)
{
    if (uart_is_driver_installed((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM)) {
        return ESP_OK;
    }
    // Initialize VFS & UART so we can use std::cout/cin
    setvbuf(stdin, NULL, _IONBF, 0);
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM,
                                        256, 0, 0, NULL, 0));
    /* Tell VFS to use UART driver */
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    return ESP_OK;
}
