// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "generate/esp_modem_command_declare.inc"

namespace esp_modem {

GenericModule::GenericModule(std::shared_ptr<DTE> dte, const dce_config *config) :
        dte(std::move(dte)), pdp(std::make_unique<PdpContext>(config->apn)) {}


#define ARGS0
#define ARGS1 , x
#define ARGS2 , x , y
#define _ARGS(x)  ARGS ## x
#define ARGS(x)  _ARGS(x)
#define TEMPLATE_ARG
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, arg_nr, MUX_ARG, ...) \
     return_type GenericModule::name(__VA_ARGS__) { return esp_modem::dce_commands::name(dte.get() ARGS(arg_nr) ); }

DECLARE_ALL_COMMAND_APIS(return_type name(...) {
    forwards
    to
    esp_modem::dce_commands::name(...)
})

#undef ESP_MODEM_DECLARE_DCE_COMMAND


command_result SIM7600::get_module_name(std::string &name) {
    name = "7600";
    return command_result::OK;
}

command_result SIM800::get_module_name(std::string &name) {
    name = "800L";
    return command_result::OK;
}

command_result BG96::get_module_name(std::string &name) {
    name = "BG96";
    return command_result::OK;
}

}