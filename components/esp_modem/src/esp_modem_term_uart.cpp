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
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "uart_resource.hpp"

namespace esp_modem {

uart_resource::~uart_resource()
{
    if (port >= UART_NUM_0 && port < UART_NUM_MAX) {
        uart_driver_delete(port);
    }
}

uart_resource::uart_resource(const esp_modem_uart_term_config *config, QueueHandle_t *event_queue, int fd)
    : port(-1)
{
    esp_err_t res;

    /* Config UART */
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->baud_rate;
    uart_config.data_bits = config->data_bits;
    uart_config.parity = config->parity;
    uart_config.stop_bits = config->stop_bits;
    uart_config.flow_ctrl = (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) ? UART_HW_FLOWCTRL_CTS_RTS
                            : UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = config->source_clk;

    throw_if_esp_fail(uart_param_config(config->port_num, &uart_config), "config uart parameter failed");

    if (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        res = uart_set_pin(config->port_num, config->tx_io_num, config->rx_io_num,
                           config->rts_io_num, config->cts_io_num);
    } else {
        res = uart_set_pin(config->port_num, config->tx_io_num, config->rx_io_num,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    throw_if_esp_fail(res, "config uart gpio failed");
    /* Set flow control threshold */
    if (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        res = uart_set_hw_flow_ctrl(config->port_num, UART_HW_FLOWCTRL_CTS_RTS, UART_FIFO_LEN - 8);
    } else if (config->flow_control == ESP_MODEM_FLOW_CONTROL_SW) {
        res = uart_set_sw_flow_ctrl(config->port_num, true, 8, UART_FIFO_LEN - 8);
    }
    throw_if_esp_fail(res, "config uart flow control failed");

    /* Install UART driver and get event queue used inside driver */
    res = uart_driver_install(config->port_num,
                              config->rx_buffer_size, config->tx_buffer_size,
                              config->event_queue_size, config->event_queue_size ?  event_queue : nullptr,
                              0);
    throw_if_esp_fail(res, "install uart driver failed");
    throw_if_esp_fail(uart_set_rx_timeout(config->port_num, 1), "set rx timeout failed");

    throw_if_esp_fail(uart_set_rx_full_threshold(config->port_num, 64), "config rx full threshold failed");

    /* mark UART as initialized */
    port = config->port_num;
}

} // namespace esp_modem
