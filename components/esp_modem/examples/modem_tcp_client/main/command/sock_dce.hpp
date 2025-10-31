/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include <cxx_include/esp_modem_dce_factory.hpp>
#include "sock_commands.hpp"
#include "sock_module.hpp"
#include <sys/socket.h>

#pragma  once

namespace sock_dce {


enum class status {
    IDLE,
    CONNECTING,
    SENDING,
    RECEIVING,
    FAILED,
    PENDING
};

class Responder {
public:
    enum class ret {
        OK, FAIL, IN_PROGRESS, NEED_MORE_DATA, NEED_MORE_TIME
    };
    Responder(int &s, int &ready_fd, std::shared_ptr<esp_modem::DTE> &dte_arg):
        sock(s), data_ready_fd(ready_fd), dte(dte_arg) {}
    ret process_data(status state, uint8_t *data, size_t len);
    ret check_async_replies(status state, std::string_view &response);
    ret check_urc(status state, std::string_view &response);

    void start_sending(size_t len);
    void start_receiving(size_t len);
    bool start_connecting(std::string host, int port);
    status pending();
    uint8_t *get_buf()
    {
        return &buffer[0];
    }
    size_t get_buf_len()
    {
        return buffer_size;
    }

    void clear_offsets()
    {
        actual_read = 0;
    }

    size_t data_available()
    {
        return actual_read;
    }

    size_t has_data()
    {
        return total_len;
    }

    // Unique link identifier used to target multi-connection AT commands
    int link_id{s_link_id++};
    // Shared mutex guarding DTE access across concurrent DCE instances
    static SemaphoreHandle_t s_dte_mutex;
private:
    static int s_link_id;
    static constexpr size_t buffer_size = 512;

    bool on_read(char *data, size_t len)
    {
#ifndef CONFIG_EXAMPLE_CUSTOM_TCP_TRANSPORT
        ::send(sock, data, len, 0);
        printf("sending %d\n", len);
#else
        ::memcpy(&buffer[actual_read], data, len);
        actual_read += len;
#endif
        return true;
    }

    ret recv(uint8_t *data, size_t len);
    ret send(uint8_t *data, size_t len);
    ret send(std::string_view response);
    ret connect(std::string_view response);
    void send_cmd(std::string_view command)
    {
        dte->write((uint8_t *) command.begin(), command.size());
    }
    std::array<uint8_t, buffer_size> buffer;
    size_t data_to_recv = 0;
    size_t actual_read = 0;
    size_t total_len = 0;

    bool read_again = false;
    int &sock;
    int &data_ready_fd;
    int send_stat = 0;
    size_t data_to_send = 0;
    std::shared_ptr<esp_modem::DTE> &dte;
};

class DCE : public Module {
    using Module::Module;
public:
    DCE(std::shared_ptr<esp_modem::DTE> dte_arg, const esp_modem_dce_config *config);
    ~DCE();

    /**
     * @brief Opens network in AT command mode
     * @return OK, FAIL or TIMEOUT
     */
    esp_modem::command_result net_open();
    /**
     * @brief Closes network in AT command mode
     * @return OK, FAIL or TIMEOUT
     */
    esp_modem::command_result net_close();
    /**
     * @brief Opens a TCP connection
     * @param[in] host Host name or IP address to connect to
     * @param[in] port Port number
     * @param[in] timeout Connection timeout
     * @return OK, FAIL or TIMEOUT
     */
    esp_modem::command_result tcp_open(const std::string &host, int port, int timeout);
    /**
     * @brief Closes opened TCP socket
     * @return OK, FAIL or TIMEOUT
     */
    esp_modem::command_result tcp_close();
    /**
     * @brief Gets modem IP address
     * @param[out] addr String representation of modem's IP
     * @return OK, FAIL or TIMEOUT
     */
    esp_modem::command_result get_ip(std::string &addr);
    /**
     * @brief Sets Rx mode
     * @param[in] mode 0=auto, 1=manual
     * @return OK, FAIL or TIMEOUT
     */
    esp_modem::command_result set_rx_mode(int mode);
    bool init();
    bool connect(std::string host, int port);
    void start_listening(int port);
    bool perform_sock();
    void set_idle()
    {
        signal.set(IDLE);
    }
    bool wait_to_idle(uint32_t ms)
    {
        if (!signal.wait(IDLE, ms)) {
            ESP_LOGE("dce", "Failed to get idle");
            return false;
        }
        if (state != status::IDLE) {
            ESP_LOGE("dce", "Unexpected state %d", static_cast<int>(state));
            return false;
        }
        return true;
    }
    int sync_recv(char *buffer, int len, int timeout_ms)
    {
        if (!wait_to_idle(timeout_ms)) {
            return 0;
        }
        at.clear_offsets();
        // Take DTE mutex before issuing receive on this link
        ESP_LOGD("TAG", "TAKE RECV %d", at.link_id);
        xSemaphoreTake(at.s_dte_mutex, portMAX_DELAY);
        ESP_LOGD("TAG", "TAKEN RECV %d", at.link_id);
        state = status::RECEIVING;
        uint64_t data;
        read(data_ready_fd, &data, sizeof(data));
        int max_len = std::min(len, (int)at.get_buf_len());
        at.start_receiving(max_len);
        if (!signal.wait(IDLE, 500 + timeout_ms)) {
            return 0;
        }
        int ret = at.data_available();
        if (ret > 0) {
            memcpy(buffer, at.get_buf(), ret);
        }
        set_idle();
        return ret;
    }
    int sync_send(const char *buffer, size_t len, int timeout_ms)
    {
        int len_to_send = std::min(len, at.get_buf_len());
        if (!wait_to_idle(timeout_ms)) {
            return -1;
        }
        // Take DTE mutex before issuing send on this link
        ESP_LOGD("TAG", "TAKE SEND %d", at.link_id);
        xSemaphoreTake(at.s_dte_mutex, portMAX_DELAY);
        ESP_LOGD("TAG", "TAKEN SEND %d", at.link_id);
        state = status::SENDING;
        memcpy(at.get_buf(), buffer, len_to_send);
        ESP_LOG_BUFFER_HEXDUMP("dce", at.get_buf(), len, ESP_LOG_VERBOSE);
        at.start_sending(len_to_send);
        if (!signal.wait(IDLE, timeout_ms + 1000)) {
            if (state == status::PENDING) {
                state = status::IDLE;
            } else {
                return -1;
            }
        }
        set_idle();
        return len_to_send;
    }
    int wait_to_read(uint32_t ms)
    {
        if (at.has_data() > 0) {
            ESP_LOGD("dce", "Data buffered in modem (len=%d)", at.has_data());
            return 1;
        }
        struct timeval tv = {
            .tv_sec = static_cast<time_t>(ms / 1000),
            .tv_usec = 0,
        };
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(data_ready_fd, &fdset);
        int s = select(data_ready_fd + 1, &fdset, nullptr, nullptr, &tv);
        if (s == 0) {
            return 0;
        } else if (s < 0) {
            ESP_LOGE("dce", "select error %d", errno);
            return -1;
        }
        if (FD_ISSET(data_ready_fd, &fdset)) {
            ESP_LOGD("dce", "select read: modem data available");
            return 1;
        }
        return -1;
    }
    static std::vector<DCE *> dce_list;
    static bool network_init;
    static void read_callback(uint8_t *data, size_t len)
    {
        for (auto dce : dce_list) {
            dce->perform_at(data, len);
        }
    }
private:
    esp_modem::SignalGroup signal;
    void close_sock();
    bool accept_sock();
    bool sock_to_at();
    bool at_to_sock();
    void perform_at(uint8_t *data, size_t len);
    status state{status::IDLE};
    static constexpr uint8_t IDLE = 1;
    Responder at{sock, data_ready_fd, dte};
    int sock {-1};
    int listen_sock {-1};
    int data_ready_fd {-1};
};
std::unique_ptr<DCE> create(const esp_modem::dce_config *config, std::shared_ptr<esp_modem::DTE> dte);
}
