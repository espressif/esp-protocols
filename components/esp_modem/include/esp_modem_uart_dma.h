/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_modem_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create UART terminal with DMA support
 *
 * This function creates a UART terminal that can use UHCI DMA for high-speed
 * data transfers. The DMA mode is enabled when the `use_dma` flag is set to true
 * in the configuration.
 *
 * @param config Configuration structure for the UART terminal
 * @return Pointer to the created terminal, or NULL on failure
 */
void* esp_modem_uart_dma_terminal_create(const esp_modem_dte_config *config);

/**
 * @brief Create UART terminal with DMA support (C++ interface)
 *
 * This is the C++ interface for creating a UART terminal with DMA support.
 * It returns a std::unique_ptr<Terminal> for automatic memory management.
 *
 * @param config Configuration structure for the UART terminal
 * @return std::unique_ptr<Terminal> for the created terminal
 */
#ifdef __cplusplus
#include "cxx_include/esp_modem_dte.hpp"
std::unique_ptr<esp_modem::Terminal> create_uart_dma_terminal(const esp_modem_dte_config *config);
#endif

#ifdef __cplusplus
}
#endif
