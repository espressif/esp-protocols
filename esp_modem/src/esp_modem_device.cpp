//
// Created by david on 3/3/21.
//

#include "cxx_include/esp_modem_dce_commands.hpp"
#include "cxx_include/esp_modem_dte.hpp"

#include "cxx_include/esp_modem_commands.hpp"



bool Device::setup_data_mode() {
    if (set_echo(false) != command_result::OK)
        return false;
    if (set_pdp_context(*pdp) != command_result::OK)
        return false;
//    if (set_data_mode() != command_result::OK)
//        return false;
    return true;
}

bool Device::set_mode(dte_mode mode) {
    if (mode == dte_mode::DATA_MODE) {
        if (set_data_mode() != command_result::OK)
            return resume_data_mode() == command_result::OK;
        return true;
    } else if (mode == dte_mode::COMMAND_MODE) {
        return set_command_mode() == command_result::OK;
    } else if (mode == dte_mode::CMUX_MODE) {
        return set_cmux() == command_result::OK;
    }
    return true;
}


command_result Device::set_echo(bool on) { return esp_modem::dce_commands::set_echo(dte, on); }
command_result Device::set_data_mode() { return esp_modem::dce_commands::set_data_mode(dte); }
command_result Device::resume_data_mode() { return esp_modem::dce_commands::resume_data_mode(dte); }
command_result Device::set_pdp_context(PdpContext& pdp_context) { return esp_modem::dce_commands::set_pdp_context(dte.get(), pdp_context); }
command_result Device::set_command_mode() { return esp_modem::dce_commands::set_command_mode(dte); }
command_result Device::set_cmux() { return esp_modem::dce_commands::set_cmux(dte); }
command_result Device::get_imsi(std::string& imsi_number) { return esp_modem::dce_commands::get_imsi(dte, imsi_number); }
command_result Device::set_pin(const std::string& pin) { return esp_modem::dce_commands::set_pin(dte, pin); }
command_result Device::read_pin(bool& pin_ok) { return esp_modem::dce_commands::read_pin(dte, pin_ok); }
command_result Device::get_imei(std::string &imei) { return esp_modem::dce_commands::get_imei(dte, imei); }
command_result Device::get_module_name(std::string &name) { return esp_modem::dce_commands::get_module_name(dte, name); }
