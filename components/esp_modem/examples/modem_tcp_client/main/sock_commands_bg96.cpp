/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <charconv>
#include <cstring>
#include <sys/socket.h>
#include "sock_commands.hpp"
#include "cxx_include/esp_modem_command_library_utils.hpp"
#include "sock_dce.hpp"

static const char *TAG = "sock_commands";

namespace sock_commands {

using namespace esp_modem;

command_result net_open(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string out;
    auto ret = dce_commands::generic_get_string(t, "AT+QISTATE?\r", out, 1000);
    if (ret != command_result::OK) {
        return ret;
    }
    if (out.find("+QISTATE: 0") != std::string::npos) {
        ESP_LOGV(TAG, "%s", out.data() );
        ESP_LOGD(TAG, "Already there");
        return command_result::OK;
    } else if (out.empty()) {
        return dce_commands::generic_command(t, "AT+QIACT=1\r", "OK", "ERROR", 150000);
    }
    return command_result::FAIL;
}

command_result net_close(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return dce_commands::generic_command(t, "AT+QIDEACT=1\r", "OK", "ERROR", 40000);
}

command_result tcp_open(CommandableIf *t, const std::string &host, int port, int timeout)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string ip_open = R"(AT+QIOPEN=1,0,"TCP",")" + host + "\"," + std::to_string(port) + "\r";
    auto ret = dce_commands::generic_command(t, ip_open, "+QIOPEN: 0,0", "ERROR", timeout);
    if (ret != command_result::OK) {
        ESP_LOGE(TAG, "%s Failed", __func__ );
        return ret;
    }
    return command_result::OK;
}

command_result tcp_close(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return dce_commands::generic_command(t, "AT+QICLOSE=0\r", "OK", "ERROR", 10000);
}

command_result tcp_send(CommandableIf *t, uint8_t *data, size_t len)
{
    ESP_LOGV(TAG, "%s", __func__ );
    assert(0);      // Remove when fix done
    return command_result::FAIL;
}

command_result tcp_recv(CommandableIf *t, uint8_t *data, size_t len, size_t &out_len)
{
    ESP_LOGV(TAG, "%s", __func__ );
    assert(0);      // Remove when fix done
    return command_result::FAIL;
}

command_result get_ip(CommandableIf *t, std::string &ip)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string out;
    auto ret = dce_commands::generic_get_string(t, "AT+QIACT?\r", out, 5000);
    if (ret != command_result::OK) {
        return ret;
    }
    auto pos = out.find("+QIACT: 1");
    auto property = 0;
    while (pos != std::string::npos) {
        // Looking for: +QIACT: <contextID>,<context_state>,<context_type>,<IP_address>
        if (property++ == 3) {  // ip is after 3rd comma (as a 4rd property of QIACT string)
            ip = out.substr(++pos);
            // strip quotes if present
            auto quote1 = ip.find('"');
            auto quote2 = ip.rfind('"');
            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                ip = ip.substr(quote1 + 1, quote2 - 1);
            }
            return command_result::OK;
        }
        pos = out.find(',', ++pos);
    }
    return command_result::FAIL;
}

} // sock_commands

namespace sock_dce {

void Listener::start_sending(size_t len)
{
    data_to_send = len;
    send_stat = 0;
    send_cmd("AT+QISEND=0," + std::to_string(len) + "\r");
}

void Listener::start_receiving(size_t len)
{
    send_cmd("AT+QIRD=0," + std::to_string(size) + "\r");
}

bool Listener::start_connecting(std::string host, int port)
{
    send_cmd(R"(AT+QIOPEN=1,0,"TCP",")" + host + "\"," + std::to_string(port) + "\r");
    return true;
}

Listener::state Listener::recv(uint8_t *data, size_t len)
{
    const size_t MIN_MESSAGE = 6;
    const std::string_view head = "+QIRD: ";
    auto head_pos = (char *)std::search(data, data + len, head.begin(), head.end());
    if (head_pos == nullptr) {
        return state::FAIL;
    }

    auto next_nl = (char *)memchr(head_pos + head.size(), '\n', MIN_MESSAGE);
    if (next_nl == nullptr) {
        return state::FAIL;
    }

    size_t actual_len;
    if (std::from_chars(head_pos + head.size(), next_nl, actual_len).ec == std::errc::invalid_argument) {
        ESP_LOGE(TAG, "cannot convert");
        return state::FAIL;
    }

    ESP_LOGD(TAG, "Received: actual len=%d", actual_len);
    if (actual_len == 0) {
        ESP_LOGD(TAG, "no data received");
        return state::FAIL;
    }

    // TODO improve : compare *actual_len* & data size (to be sure that received data is equal to *actual_len*)
    if (actual_len > size) {
        ESP_LOGE(TAG, "TOO BIG");
        return state::FAIL;
    }
    ::send(sock, next_nl + 1, actual_len, 0);

    // "OK" after the data
    auto last_pos = (char *)memchr(next_nl + 1 + actual_len, 'O', MIN_MESSAGE);
    if (last_pos == nullptr || last_pos[1] != 'K') {
        return state::FAIL;
    }
    if ((char *)data + len - last_pos > MIN_MESSAGE) {
        // check for async replies after the Recv header
        std::string_view response((char *)last_pos + 2 /* OK */, (char *)data + len - last_pos);
        check_async_replies(response);
    }
    return state::OK;
}


Listener::state Listener::send(uint8_t *data, size_t len)
{
    if (send_stat == 0) {
        if (memchr(data, '>', len) == NULL) {
            ESP_LOGE(TAG, "Missed >");
            return state::FAIL;
        }
        auto written = dte->write(&buffer[0], data_to_send);
        if (written != data_to_send) {
            ESP_LOGE(TAG, "written %d (%d)...", written, len);
            return state::FAIL;
        }
        data_to_send = 0;
        send_stat++;
    }
    return Listener::state::IN_PROGRESS;
}

Listener::state Listener::send(std::string_view response)
{
    if (send_stat == 1) {
        if (response.find("SEND OK") != std::string::npos) {
            send_cmd("AT+QISEND=0,0\r");
            send_stat++;
        } else if (response.find("SEND FAIL") != std::string::npos) {
            ESP_LOGE(TAG, "Sending buffer full");
            return state::FAIL;
        } else if (response.find("ERROR") != std::string::npos) {
            ESP_LOGE(TAG, "Failed to sent");
            return state::FAIL;
        }
    } else if (send_stat == 2) {
        constexpr std::string_view head = "+QISEND: ";
        if (response.find(head) != std::string::npos) {
            // Parsing +QISEND: <total_send_length>,<ackedbytes>,<unackedbytes>
            size_t head_pos = response.find(head);
            response = response.substr(head_pos + head.size());
            int pos, property = 0;
            int total = 0, ack = 0, unack = 0;
            while ((pos = response.find(',')) != std::string::npos) {
                auto next_comma = (char *)memchr(response.data(), ',', response.size());

                // extract value
                size_t value;
                if (std::from_chars(response.data(), next_comma, value).ec == std::errc::invalid_argument) {
                    ESP_LOGE(TAG, "cannot convert");
                    return state::FAIL;
                }

                switch (property++) {
                case 0: total = value;
                    break;
                case 1: ack = value;
                    break;
                default:
                    return state::FAIL;
                }
                response = response.substr(pos + 1);
            }
            if (std::from_chars(response.data(), response.data() + pos, unack).ec == std::errc::invalid_argument) {
                return state::FAIL;
            }

            // TODO improve : need check *total* & *ack* values, or loop (every 5 sec) with 90s or 120s timeout
            if (ack < total) {
                ESP_LOGE(TAG, "all sending data are not ack (missing %d bytes acked)", (total - ack));
            }
            return state::OK;
        } else if (response.find("ERROR") != std::string::npos) {
            ESP_LOGE(TAG, "Failed to check sending");
            return state::FAIL;
        }

    }
    return Listener::state::IN_PROGRESS;
}

Listener::state Listener::connect(std::string_view response)
{
    if (response.find("+QIOPEN: 0,0") != std::string::npos) {
        ESP_LOGI(TAG, "Connected!");
        return state::OK;
    }
    if (response.find("ERROR") != std::string::npos) {
        ESP_LOGE(TAG, "Failed to open");
        return state::FAIL;
    }
    return Listener::state::IN_PROGRESS;
}

void Listener::check_async_replies(std::string_view &response) const
{
    ESP_LOGD(TAG, "response %.*s", static_cast<int>(response.size()), response.data());
    if (response.find("+QIURC: \"recv\",0") != std::string::npos) {
        uint64_t data_ready = 1;
        write(data_ready_fd, &data_ready, sizeof(data_ready));
        ESP_LOGD(TAG, "Got data on modem!");
    }
}


} // sock_dce
