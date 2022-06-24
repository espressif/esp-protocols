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

#define ESP_MODEM_VFS_DEFAULT_UART_CONFIG(name)  {  \
        .dev_name = (name), \
        .uart = {               \
            .port_num = UART_NUM_1,                 \
            .data_bits = UART_DATA_8_BITS,          \
            .stop_bits = UART_STOP_BITS_1,          \
            .parity = UART_PARITY_DISABLE,          \
            .flow_control = ESP_MODEM_FLOW_CONTROL_NONE,\
            .source_clk = UART_SCLK_APB,            \
            .baud_rate = 115200,                    \
            .tx_io_num = 25,                        \
            .rx_io_num = 26,                        \
            .rts_io_num = 27,                       \
            .cts_io_num = 23,                       \
            .rx_buffer_size = 4096,                 \
            .tx_buffer_size = 512,                  \
            .event_queue_size = 0,                  \
       },                                           \
}

/**
 * @brief UART init struct for VFS
 */
struct esp_modem_vfs_uart_creator {
    const char *dev_name;                       /*!< VFS device name, e.g. /dev/uart/n */
    const struct esp_modem_uart_term_config uart;     /*!< UART driver init struct */
};

/**
 * @brief UART init struct for VFS
 */
struct esp_modem_vfs_socket_creator {
    const char *host_name;                    /*!< VFS socket: host name (or IP address) */
    int port;                                 /*!< VFS socket: port number */
};

/**
 * @brief Creates a socket VFS and configures the DTE struct
 *
 * @param config Socket config option, basically host + port
 * @param created_config reference to the VFS portion of the DTE config to be set up
 * @return true on success
 */
bool vfs_create_socket(struct esp_modem_vfs_socket_creator *config, struct esp_modem_vfs_term_config *created_config);

/**
 * @brief Creates a uart VFS and configures the DTE struct
 *
 * @param config Uart config option, basically file name and console options
 * @param created_config reference to the VFS portion of the DTE config to be set up
 * @return true on success
 */
bool vfs_create_uart(struct esp_modem_vfs_uart_creator *config, struct esp_modem_vfs_term_config *created_config);
