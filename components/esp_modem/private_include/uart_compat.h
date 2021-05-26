// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "esp_log.h"
#include "driver/uart.h"

/**
 * @brief This is a compatible header, which just takes care of different data ptr type
 * across different IDF version in driver/uart
 */
static inline int uart_write_bytes_compat(uart_port_t uart_num, const void* src, size_t size)
{
#if ESP_IDF_VERSION_MAJOR >= 4 && ESP_IDF_VERSION_MINOR >= 3
    const void *data = src;
#else
    auto *data = reinterpret_cast<const char*>(src);
#endif
    return uart_write_bytes(uart_num, data, size);
}
