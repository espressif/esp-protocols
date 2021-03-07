//
// Created by david on 3/8/21.
//

#include "cxx_include/esp_modem_dce_module.hpp"

command_result SIM7600::get_module_name(std::string& name)
{
    name = "7600";
    return command_result::OK;
}

command_result SIM800::get_module_name(std::string& name)
{
    name = "800L";
    return command_result::OK;
}

command_result BG96::get_module_name(std::string& name)
{
    name = "BG96";
    return command_result::OK;
}
