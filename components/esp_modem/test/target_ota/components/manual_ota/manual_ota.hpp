/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once
#include <vector>
#include "esp_http_client.h"
#include "esp_partition.h"
#include "esp_transport_tcp.h"
#include "esp_ota_ops.h"

class manual_ota {
public:
    enum class state {
        UNDEF,
        INIT,
        IMAGE_CHECK,
        START,
        END,
        FAIL,
    };
    size_t size_{32};
    int timeout_{2};

    /**
     * @brief Construct a new manual ota object
     *
     * @param uri URI of the binary image
     */
    explicit manual_ota(const char *uri): uri_(uri) {}

    /**
     * @brief Start the manual OTA process
     *
     * @return true if started successfully
     */
    bool begin();

    /**
     * @brief Performs one read-write OTA iteration
     *
     * @return true if the process is in progress
     * @return false if the process finished, call end() to get OTA result
     */
    bool perform();

    /**
     * @brief Finishes an OTA update
     *
     * @return true if the OTA update completed successfully
     */
    bool end();

private:
    const char *uri_{};
    esp_http_client_handle_t http_;
    int64_t image_length_;
    size_t file_length_;
    const size_t max_buffer_size_{size_ * 1024};
    const esp_partition_t *partition_{nullptr};
    state status{state::UNDEF};
    std::vector<char> buffer_{};
    int reconnect_attempts_;
    const int max_reconnect_attempts_{3};
    esp_transport_handle_t ssl_;
    esp_ota_handle_t update_handle_{0};

    bool prepare_reconnect();
    bool fail_cleanup();
};
