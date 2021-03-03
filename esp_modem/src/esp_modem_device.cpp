//
// Created by david on 3/3/21.
//

#include "cxx_include/esp_modem_dce_commands.hpp"
#include "cxx_include/esp_modem_dte.hpp"



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
    }
    return true;
}

