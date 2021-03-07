//
// Created by david on 3/2/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_LIBRARY_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_LIBRARY_HPP

#include "esp_modem_dte.hpp"
#include "esp_modem_dce_module.hpp"
#include "esp_modem_types.hpp"
#include "generate/esp_modem_command_declare.inc"

namespace esp_modem {
namespace dce_commands {

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
        return_type name(CommandableIf *t, ## __VA_ARGS__);

        DECLARE_ALL_COMMAND_APIS(forwards name(...) { device->name(...); } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

} // dce_commands
} // esp_modem


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_LIBRARY_HPP
