/**
 * @brief Opens network in AT command mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(net_open, command_result)

/**
 * @brief Closes network in AT command mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(net_close, command_result)

/**
 * @brief Opens a TCP connection
 * @param[in] host Host name or IP address to connect to
 * @param[in] port Port number
 * @param[in] timeout Connection timeout
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(tcp_open, command_result, STR_IN(host), INT_IN(port), INT_IN(timeout))

/**
 * @brief Closes opened TCP socket
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(tcp_close, command_result)

/**
 * @brief Gets modem IP address
 * @param[out] addr String representation of modem's IP
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_ip, command_result, STR_OUT(addr))

/**
 * @brief Sets Rx mode
 * @param[in] mode 0=auto, 1=manual
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_rx_mode, command_result, INT_IN(mode))
