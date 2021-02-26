//
// Created by david on 2/24/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_CONFIG_H
#define SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_CONFIG_H

/**
 * @brief Modem flow control type
 *
 */
typedef enum {
    ESP_MODEM_FLOW_CONTROL_NONE = 0,
    ESP_MODEM_FLOW_CONTROL_SW,
    ESP_MODEM_FLOW_CONTROL_HW
} esp_modem_flow_ctrl_t;

struct dte_config {
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


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_CONFIG_H
