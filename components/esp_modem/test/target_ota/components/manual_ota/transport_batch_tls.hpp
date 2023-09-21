/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

/**
 * @brief Creates batch transport
 *
 * @param parent tcp-transport handle to the parent transport
 * @param max_buffer_size maximum size of one batch
 * @return created transport handle
 */
esp_transport_handle_t esp_transport_batch_tls_init(esp_transport_handle_t parent, const size_t max_buffer_size);

/**
 * @brief Performs batch read operation from the underlying transport
 *
 * @param t Transport handle
 * @param len Batch size
 * @param timeout_ms Timeout in ms
 * @return true If read from the parent transport completed successfully
 */
bool esp_transport_batch_tls_pre_read(esp_transport_handle_t t, int len, int timeout_ms);
