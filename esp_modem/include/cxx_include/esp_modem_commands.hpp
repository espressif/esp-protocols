//
// Created by david on 3/2/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP

#include "esp_modem_dte.hpp"
#include "esp_modem_dce_commands.hpp"
#include <string.h>

enum class command_result;



#include <iostream>

namespace esp_modem {
namespace dce_commands {

template <typename T> static inline command_result generic_command(T t, const std::string& command,
                                                                   const std::string& pass_phrase,
                                                                   const std::string& fail_phrase, uint32_t timeout_ms)
{
    std::cout << command << std::endl;
    return t->command(command.c_str(), [&](uint8_t *data, size_t len) {
        std::string response((char*)data, len);
        std::cout << response << std::endl;

        if (response.find(pass_phrase) != std::string::npos) {
            return command_result::OK;
        } else if (response.find(fail_phrase) != std::string::npos) {
            return command_result::FAIL;
        }
        return command_result::TIMEOUT;
    }, timeout_ms);
}
static inline void strip_cr_lf_tail(char *str, uint32_t len)
{
    if (str[len - 2] == '\r') {
        str[len - 2] = '\0';
    } else if (str[len - 1] == '\r') {
        str[len - 1] = '\0';
    }
}

template <typename T> static inline command_result generic_get_string(T t, const std::string& command, std::string& output, uint32_t timeout_ms)
{
    std::cout << command << std::endl;
    return t->command(command.c_str(), [&](uint8_t *data, size_t len) {
        size_t pos = 0;
        std::string response((char*)data, len);
        while ((pos = response.find('\n')) != std::string::npos) {
            std::string token = response.substr(0, pos);
            for (auto i = 0; i<2; ++i)  // trip trailing \n\r of last two chars
                if (pos >= 1 && (token[pos-1] == '\r' || token[pos-1] == '\n'))
                    token.pop_back();
            std::cout << "###" << token << "#### " << std::endl;

            if (token.find("OK") != std::string::npos) {
                return command_result::OK;
            } else if (token.find("ERROR") != std::string::npos) {
                return command_result::FAIL;
            } else if (token.length() > 2) {
//            response.resize(response.find('\r'));
//            response.erase(std::find(response.begin(), response.end(), '\r'), response.end());
//                std::cout << "|" << output << "|" << std::endl;
//                const std::string& out(response);
                output = token; //.substr(0,-1);
                std::cout << "|" << token << "|" << std::endl;
//                std::cout << output << std::endl;
            }
            response = response.substr(pos+1);
        }
        return command_result::TIMEOUT;
    }, timeout_ms);
}

template <typename T> static inline command_result generic_command_common(T t, std::string command)
{
    return generic_command(t, command, "OK", "ERROR", 500);
}


template <typename T> command_result sync(T t)
{
    return generic_command_common(t, "AT\r");
}

template <typename T> command_result set_echo(T t, bool on)
{
    if (on)
        return generic_command_common(t, "ATE1\r");
    return generic_command_common(t, "ATE0\r");
}

template <typename T> command_result set_pdp_context(T t, PdpContext& pdp)
{
    std::string pdp_command = "AT+CGDCONT=" + std::to_string(pdp.context_id) +
                              ",\"" + pdp.protocol_type + "\",\"" + pdp.apn + "\"\r";
    return generic_command_common(t, pdp_command);
}

template <typename T> command_result set_data_mode(T t)
{
    return generic_command(t, "ATD*99##\r", "CONNECT", "ERROR", 5000);
}

template <typename T> command_result resume_data_mode(T t)
{
    return generic_command(t, "ATO\r", "CONNECT", "ERROR", 5000);
}

template <typename T> command_result set_command_mode(T t)
{
    std::cout << "Sending +++" << std::endl;
    return t->command("+++", [&](uint8_t *data, size_t len) {
        std::string response((char*)data, len);
        std::cout << response << std::endl;
        if (response.find("OK") != std::string::npos) {
            return command_result::OK;
        } else if (response.find("NO CARRIER") != std::string::npos) {
            return command_result::OK;
        } else if (response.find("ERROR") != std::string::npos) {
            return command_result::FAIL;
        }
        return command_result::TIMEOUT;
    }, 5000);
}

template <typename T> command_result get_imsi(T t, std::string& imsi_number)
{
    return generic_get_string(t, "AT+CIMI\r", imsi_number, 5000);
}

template <typename T> command_result get_imei(T t, std::string& out)
{
    return generic_get_string(t, "AT+CGSN\r", out, 5000);
}

template <typename T> command_result get_module_name(T t, std::string& out)
{
    return generic_get_string(t, "AT+CGMM\r", out, 5000);
}

template <typename T> command_result set_cmux(T t)
{
    return generic_command_common(t, "AT+CMUX=0\r");
}

template <typename T> command_result read_pin(T t, bool& pin_ok)
{
    std::cout << "Sending read_pin" << std::endl;
    return t->command("AT+CPIN?\r", [&](uint8_t *data, size_t len) {
        std::string response((char*)data, len);
        std::cout << response << std::endl;
        if (response.find("READY") != std::string::npos) {
            pin_ok = true;
            return command_result::OK;
        } else if (response.find("PIN") != std::string::npos || response.find("PUK") != std::string::npos ) {
            pin_ok = false;
            return command_result::OK;
        } else if (response.find("ERROR") != std::string::npos) {
            return command_result::FAIL;
        }
        return command_result::TIMEOUT;
    }, 5000);
}

template <typename T> command_result set_pin(T t, const std::string& pin)
{
    std::cout << "Sending set_pin" << std::endl;
    std::string set_pin_command = "AT+CPIN=" + pin + "\r";
    return generic_command_common(t, set_pin_command);
}

} // dce_commands
} // esp_modem


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP
