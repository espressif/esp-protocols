/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include <cxx_include/esp_modem_dce_factory.hpp>
#include "socket_commands.inc"
#include "sock_commands.hpp"

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
private:
    static constexpr size_t buffer_size = 512;

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
    bool read_again = false;
    int &sock;
    int &data_ready_fd;
    int send_stat = 0;
    size_t data_to_send = 0;
    std::shared_ptr<esp_modem::DTE> &dte;
};

class DCE : public ::esp_modem::GenericModule {
    using esp_modem::GenericModule::GenericModule;
public:

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
esp_modem::return_type name(__VA_ARGS__);

    DECLARE_SOCK_COMMANDS(declare name(Commandable *p, ...);)

#undef ESP_MODEM_DECLARE_DCE_COMMAND

    bool init_network();
    bool start(std::string host, int port);

    void init_sock(int port);

    bool perform_sock();

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
