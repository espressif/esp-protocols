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

#ifndef _UART_RESOURCE_HPP_
#define _UART_RESOURCE_HPP_

#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"

struct esp_modem_dte_config;

namespace esp_modem {

/**
 * @brief Uart Resource is a platform specific struct which is implemented separately for ESP_PLATFORM and linux target
 */
struct uart_resource {
    explicit uart_resource(const esp_modem_dte_config *config, struct QueueDefinition** event_queue, int fd);
    ~uart_resource();
    uart_port_t port{};
};

std::unique_ptr<Terminal> create_vfs_terminal(const esp_modem_dte_config *config);

}  // namespace esp_modem

#endif // _UART_RESOURCE_HPP_
