//
// Created by david on 3/2/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP

#include "esp_modem_dte.hpp"
//#include "esp_modem_dce_commands.hpp"
enum class command_result;
struct PdpContext {
    PdpContext(std::string& apn): context_id(1), protocol_type("IP"), apn(apn) {}
    size_t context_id;
    std::string protocol_type;
    std::string apn;
};


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
    return generic_command(t, "+++", "OK", "ERROR", 50000);
}



} // dce_commands
} // esp_modem


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP
