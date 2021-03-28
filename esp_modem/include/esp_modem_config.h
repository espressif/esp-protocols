//
// Created by david on 2/24/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_CONFIG_H
#define SIMPLE_CXX_CLIENT_ESP_MODEM_CONFIG_H
#include "driver/uart.h"
#include "esp_modem_dce_config.h"

/**
 * @brief Modem flow control type
 *
 */
typedef enum {
    ESP_MODEM_FLOW_CONTROL_NONE = 0,
    ESP_MODEM_FLOW_CONTROL_SW,
    ESP_MODEM_FLOW_CONTROL_HW
} esp_modem_flow_ctrl_t;

struct esp_modem_dte_config {
    uart_port_t port_num;           /*!< UART port number */
    uart_word_length_t data_bits;   /*!< Data bits of UART */
    uart_stop_bits_t stop_bits;     /*!< Stop bits of UART */
    uart_parity_t parity;           /*!< Parity type */
    esp_modem_flow_ctrl_t flow_control; /*!< Flow control type */
    int baud_rate;             /*!< Communication baud rate */
    int tx_io_num;                  /*!< TXD Pin Number */
    int rx_io_num;                  /*!< RXD Pin Number */
    int rts_io_num;                 /*!< RTS Pin Number */
    int cts_io_num;                 /*!< CTS Pin Number */
    int rx_buffer_size;             /*!< UART RX Buffer Size */
    int tx_buffer_size;             /*!< UART TX Buffer Size */
    int pattern_queue_size;         /*!< UART Pattern Queue Size */
    int event_queue_size;           /*!< UART Event Queue Size */
    uint32_t event_task_stack_size; /*!< UART Event Task Stack size */
    int event_task_priority;        /*!< UART Event Task Priority */
    int line_buffer_size;           /*!< Line buffer size for command mode */
};

/**
 * @brief ESP Modem DTE Default Configuration
 *
 */
#define ESP_MODEM_DTE_DEFAULT_CONFIG()          \
    {                                           \
        .port_num = UART_NUM_1,                 \
        .data_bits = UART_DATA_8_BITS,          \
        .stop_bits = UART_STOP_BITS_1,          \
        .parity = UART_PARITY_DISABLE,          \
        .flow_control = ESP_MODEM_FLOW_CONTROL_NONE,\
        .baud_rate = 115200,                    \
        .tx_io_num = 25,                        \
        .rx_io_num = 26,                        \
        .rts_io_num = 27,                       \
        .cts_io_num = 23,                       \
        .rx_buffer_size = 1024,                 \
        .tx_buffer_size = 512,                  \
        .pattern_queue_size = 20,               \
        .event_queue_size = 30,                 \
        .event_task_stack_size = 4096,          \
        .event_task_priority = 5,               \
        .line_buffer_size = 512                 \
    }

typedef struct esp_modem_dte_config esp_modem_dte_config_t;

#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_CONFIG_H
