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


#include "cxx_include/esp_modem_dte.hpp"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "esp_vfs_dev.h"
#include <sys/fcntl.h>



static const char *TAG = "uart_terminal";

namespace esp_modem::terminal {

struct uart_resource {
    explicit uart_resource(const esp_modem_dte_config *config);
    ~uart_resource();
    uart_port_t port;
    int fd;
};

uart_resource::uart_resource(const esp_modem_dte_config *config) :
        port(-1), fd(-1)
{
    /* Config UART */
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->vfs_config.baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_REF_TICK;

    throw_if_esp_fail(uart_param_config(config->vfs_config.port_num, &uart_config), "config uart parameter failed");

    throw_if_esp_fail(uart_set_pin(config->vfs_config.port_num, config->vfs_config.tx_io_num, config->vfs_config.rx_io_num,
                                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), "config uart gpio failed");

    throw_if_esp_fail(uart_driver_install(config->vfs_config.port_num, config->vfs_config.rx_buffer_size, config->vfs_config.tx_buffer_size,
                                          0, nullptr, 0), "install uart driver failed");

//    throw_if_esp_fail(uart_set_rx_timeout(config->vfs_config.port_num, 1), "set rx timeout failed");
//
//    throw_if_esp_fail(uart_set_rx_full_threshold(config->uart_config.port_num, 64), "config rx full threshold failed");

    /* mark UART as initialized */
    port = config->vfs_config.port_num;
    esp_vfs_dev_uart_use_driver(port);

    fd = open(config->vfs_config.dev_name, O_RDWR);

    throw_if_false(fd >= 0, "Cannot open the fd");
}

uart_resource::~uart_resource() {
    if (port >= UART_NUM_0 && port < UART_NUM_MAX) {
        uart_driver_delete(port);
    }

}

} // namespace esp_modem::terminal
