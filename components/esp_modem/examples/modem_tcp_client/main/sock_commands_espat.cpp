/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <charconv>
#include <cstring>
#include "sock_commands.hpp"
#include "cxx_include/esp_modem_command_library_utils.hpp"
#include "sock_dce.hpp"
#include "sock_module.hpp"

static const char *TAG = "sock_commands_espat";

namespace sock_dce {

using namespace esp_modem;

command_result Module::sync()
{
    return dce_commands::generic_command_common(dte.get(), "AT\r\n");
}

command_result Module::set_echo(bool on)
{
    return dce_commands::generic_command_common(dte.get(), "ATE0\r\n");
}

command_result Module::set_pdp_context(PdpContext &pdp)
{
    return command_result::OK;
}

}

namespace sock_commands {

using namespace esp_modem;

command_result net_open(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__);

    // Set WiFi mode to station
    auto ret = dce_commands::generic_command(t, "AT+CWMODE=1\r\n", "OK", "ERROR", 5000);
    if (ret != command_result::OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode");
        return ret;
    }

    // Connect to WiFi network
    std::string wifi_cmd = "AT+CWJAP=\"" CONFIG_EXAMPLE_WIFI_SSID "\",\"" CONFIG_EXAMPLE_WIFI_PASSWORD "\"\r\n";
    ret = dce_commands::generic_command(t, wifi_cmd, "OK", "ERROR", 15000);
    if (ret != command_result::OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ret;
    }
    ESP_LOGI(TAG, "WiFi connected successfully");

    // Enable multiple connections mode
    ret = dce_commands::generic_command(t, "AT+CIPMUX=1\r\n", "OK", "ERROR", 1000);
    if (ret != command_result::OK) {
        ESP_LOGE(TAG, "Failed to enable multiple connections mode");
        return ret;
    }
    ESP_LOGD(TAG, "Multiple connections mode enabled");

    // Set passive receive mode (1) for better control
    for (int i = 0; i < 2; i++) {
        std::string cmd = "AT+CIPRECVTYPE=" + std::to_string(i) + ",1\r\n";
        dce_commands::generic_command(t, cmd, "OK", "ERROR", 1000);
    }
    return command_result::OK;
}

command_result net_close(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__);
    // Disconnect from WiFi
    auto ret = dce_commands::generic_command(t, "AT+CWQAP\r\n", "OK", "ERROR", 5000);
    if (ret != command_result::OK) {
        ESP_LOGW(TAG, "Failed to disconnect WiFi (may already be disconnected)");
    }
    return command_result::OK;
}

command_result tcp_close(CommandableIf *t)
{
    return command_result::OK;
    ESP_LOGV(TAG, "%s", __func__);
    // Use link ID 0 for closing connection
    const int link_id = 0;
    std::string close_cmd = "AT+CIPCLOSE=" + std::to_string(link_id) + "\r\n";

    // In multiple connections mode, response format is: <link ID>,CLOSED
    std::string expected_response = std::to_string(link_id) + ",CLOSED";

    return dce_commands::generic_command(t, close_cmd, expected_response, "ERROR", 5000);
}


command_result get_ip(CommandableIf *t, std::string &ip)
{
    ESP_LOGV(TAG, "%s", __func__);
    std::string out;
    auto ret = dce_commands::at_raw(t, "AT+CIFSR\r\n", out, "OK", "ERROR", 5000);
    if (ret != command_result::OK) {
        return ret;
    }

    // Parse station IP from response
    // Expected format: +CIFSR:STAIP,"192.168.1.100"
    auto pos = out.find("+CIFSR:STAIP,\"");
    if (pos != std::string::npos) {
        pos += 14; // Move past "+CIFSR:STAIP,\""
        auto end_pos = out.find("\"", pos);
        if (end_pos != std::string::npos) {
            ip = out.substr(pos, end_pos - pos);
            ESP_LOGI(TAG, "Got IP address: %s", ip.c_str());
            return command_result::OK;
        }
    }

    ESP_LOGE(TAG, "Failed to parse IP address from response");
    return command_result::FAIL;
}

command_result set_rx_mode(CommandableIf *t, int mode)
{
    ESP_LOGV(TAG, "%s", __func__);
    // For multiple connections mode, set receive mode for link ID 0
    const int link_id = 0;
    // Active mode (0) sends data automatically, Passive mode (1) notifies about data for reading
    std::string cmd = "AT+CIPRECVTYPE=" + std::to_string(link_id) + "," + std::to_string(mode) + "\r\n";
    return dce_commands::generic_command(t, cmd, "OK", "ERROR", 1000);
}

} // sock_commands

namespace sock_dce {

void Responder::start_sending(size_t len)
{
    data_to_send = len;
    send_stat = 0;
    // For multiple connections mode, include link ID
    send_cmd("AT+CIPSEND=" + std::to_string(link_id) + "," + std::to_string(len) + "\r\n");
}

void Responder::start_receiving(size_t len)
{
    // For multiple connections mode, include link ID
    send_cmd("AT+CIPRECVDATA=" + std::to_string(link_id) + "," + std::to_string(len) + "\r\n");
}

bool Responder::start_connecting(std::string host, int port)
{
    // For multiple connections mode, include link ID
    std::string cmd = "AT+CIPSTART=" + std::to_string(link_id) + ",\"TCP\",\"" + host + "\"," + std::to_string(port) + "\r\n";
    send_cmd(cmd);
    return true;
}

Responder::ret Responder::recv(uint8_t *data, size_t len)
{
    const int MIN_MESSAGE = 6;
    size_t actual_len = 0;
    auto *recv_data = (char *)data;

    if (data_to_recv == 0) {
        const std::string_view head = "+CIPRECVDATA:";
        const std::string_view recv_data_view(recv_data, len);
        const auto head_pos_found = recv_data_view.find(head);
        if (head_pos_found == std::string_view::npos) {
            return ret::IN_PROGRESS;
        }
        const auto *head_pos = recv_data + head_pos_found;
        // Find the end of the length field
        auto next_comma = (char *)memchr(head_pos + head.size(), ',', MIN_MESSAGE);
        if (next_comma == nullptr) {
            return ret::FAIL;
        }

        // Parse the actual length
        if (std::from_chars(head_pos + head.size(), next_comma, actual_len).ec == std::errc::invalid_argument) {
            ESP_LOGE(TAG, "Cannot convert length");
            return ret::FAIL;
        }

        ESP_LOGD(TAG, "Received: actual len=%zu", actual_len);
        if (actual_len == 0) {
            ESP_LOGD(TAG, "No data received");
            return ret::FAIL;
        }

        if (actual_len > buffer_size) {
            ESP_LOGE(TAG, "Data too large: %zu > %zu", actual_len, buffer_size);
            return ret::FAIL;
        }

        // Move to the actual data after the comma
        recv_data = next_comma + 1;
        auto first_data_len = len - (recv_data - (char *)data);

        if (actual_len > first_data_len) {
            on_read(recv_data, first_data_len);
            data_to_recv = actual_len - first_data_len;
            return ret::NEED_MORE_DATA;
        }
        on_read(recv_data, actual_len);

    } else if (data_to_recv > len) {    // Continue receiving
        on_read(recv_data, len);
        data_to_recv -= len;
        return ret::NEED_MORE_DATA;

    } else if (data_to_recv <= len) {    // Last chunk
        on_read(recv_data, data_to_recv);
        actual_len = data_to_recv;
    }

    // Look for "OK" marker after the data
    char *ok_pos = nullptr;
    if (actual_len + 1 + 2 /* OK */ <= len) {
        ok_pos = (char *)memchr(recv_data + actual_len + 1, 'O', MIN_MESSAGE);
        if (ok_pos == nullptr) { // || ok_pos[1] != 'K') {
            data_to_recv = 0;
            ESP_LOGE(TAG, "Missed 'OK' marker");
            return ret::OK;
            return ret::FAIL;
        }
        if (ok_pos + 1 < recv_data + len && ok_pos[1] != 'K') {
            // we ignore the condition when receiving 'O' as the last character in the last batch,
            // don't wait for the 'K' in the next run, assume the data are valid and let higher layers deal with it.
            data_to_recv = 0;
            ESP_LOGE(TAG, "Missed 'OK' marker2");
            return ret::FAIL;
        }
    }
    if (ok_pos != nullptr && (char *)data + len - ok_pos - 2 > MIN_MESSAGE) {
        // check for async replies after the Recv header
        std::string_view response((char *)ok_pos + 2 /* OK */, (char *)data + len - ok_pos);
        check_urc(status::RECEIVING, response);
    }
    // Reset and prepare for next receive
    data_to_recv = 0;
    return ret::OK;
}

Responder::ret Responder::send(uint8_t *data, size_t len)
{
    if (send_stat < 3) {
        // Look for the '>' prompt
        if (memchr(data, '>', len) == NULL) {
            if (send_stat++ < 2) {
                return ret::NEED_MORE_DATA;
            }
            ESP_LOGE(TAG, "Missed '>' prompt");
            return ret::FAIL;
        }

        // Send the actual data
        auto written = dte->write(&buffer[0], data_to_send);
        if (written != data_to_send) {
            ESP_LOGE(TAG, "Failed to write data: %d/%zu", written, data_to_send);
            return ret::FAIL;
        }
        data_to_send = 0;
        send_stat = 3;
    }
    return ret::IN_PROGRESS;
}

Responder::ret Responder::send(std::string_view response)
{
    if (send_stat == 3) {
        if (response.find("SEND OK") != std::string::npos) {
            send_stat = 0;
            return ret::OK;
        } else if (response.find("SEND FAIL") != std::string::npos) {
            ESP_LOGE(TAG, "Send failed");
            return ret::FAIL;
        } else if (response.find("ERROR") != std::string::npos) {
            ESP_LOGE(TAG, "Send error");
            return ret::FAIL;
        }
    }
    return ret::IN_PROGRESS;
}

Responder::ret Responder::connect(std::string_view response)
{
    // In multiple connections mode, response format is: <link ID>,CONNECT
    if (response.find(",CONNECT") != std::string::npos || response.find("CONNECT") != std::string::npos) {
        ESP_LOGI(TAG, "TCP connected!");
        return ret::OK;
    }
    if (response.find("ERROR") != std::string::npos) {
        ESP_LOGE(TAG, "Failed to connect");
        return ret::FAIL;
    }
    return ret::IN_PROGRESS;
}
Responder::ret Responder::check_urc(status state, std::string_view &response)
{
    // Handle data notifications - in multiple connections mode, format is +IPD,<link ID>,<len>
    std::string expected_urc = "+IPD," + std::to_string(link_id);
    if (response.find(expected_urc) != std::string::npos) {
        uint64_t data_ready = 1;
        write(data_ready_fd, &data_ready, sizeof(data_ready));
        ESP_LOGD(TAG, "Data available notification");
    }
    return ret::IN_PROGRESS;
}

Responder::ret Responder::check_async_replies(status state, std::string_view &response)
{
    ESP_LOGD(TAG, "Response: %.*s", static_cast<int>(response.size()), response.data());

    // Handle WiFi status messages
    if (response.find("WIFI CONNECTED") != std::string::npos) {
        ESP_LOGI(TAG, "WiFi connected");
    } else if (response.find("WIFI DISCONNECTED") != std::string::npos) {
        ESP_LOGD(TAG, "WiFi disconnected");
    }

    // Handle TCP status messages (multiple connections format: <link ID>,CONNECT or <link ID>,CLOSED)
    if (response.find("CONNECT") != std::string::npos && state == status::CONNECTING) {
        return connect(response);
    } else if (response.find("CLOSED") != std::string::npos) {
        ESP_LOGD(TAG, "TCP connection closed");
        return ret::FAIL;
    }

    if (state == status::SENDING) {
        return send(response);
    } else if (state == status::CONNECTING) {
        return connect(response);
    }

    return ret::IN_PROGRESS;
}

Responder::ret Responder::process_data(status state, uint8_t *data, size_t len)
{
    if (state == status::SENDING) {
        return send(data, len);
    }
    if (state == status::RECEIVING) {
        return recv(data, len);
    }
    return ret::IN_PROGRESS;
}

status Responder::pending()
{
    // For ESP-AT, we don't need a pending check like BG96
    // Just return current status
    return status::SENDING;
}

} // sock_dce
