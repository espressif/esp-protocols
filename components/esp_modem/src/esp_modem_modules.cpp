/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "generate/esp_modem_command_declare.inc"
#include "cxx17_include/esp_modem_command_library_17.hpp"

namespace esp_modem {

GenericModule::GenericModule(std::shared_ptr<DTE> dte, const dce_config *config) :
    dte(std::move(dte)), pdp(std::make_unique<PdpContext>(config->apn)) {}

//
// Define preprocessor's forwarding to dce_commands definitions
//

// Helper macros to handle multiple arguments of declared API
#define ARGS0
#define ARGS1 , p1
#define ARGS2 , p1 , p2
#define ARGS3 , p1 , p2 , p3
#define ARGS4 , p1 , p2 , p3, p4
#define ARGS5 , p1 , p2 , p3, p4, p5
#define ARGS6 , p1 , p2 , p3, p4, p5, p6

#define _ARGS(x)  ARGS ## x
#define ARGS(x)  _ARGS(x)

//
// Repeat all declarations and forward to the AT commands defined in esp_modem::dce_commands:: namespace
//
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, arg_nr, ...) \
     return_type GenericModule::name(__VA_ARGS__) { return esp_modem::dce_commands::name(dte.get() ARGS(arg_nr) ); }

DECLARE_ALL_COMMAND_APIS(return_type name(...))

#undef ESP_MODEM_DECLARE_DCE_COMMAND

//
// Handle specific commands for specific supported modems
//
command_result SIM7600::get_battery_status(int &voltage, int &bcs, int &bcl)
{
    return dce_commands::get_battery_status_sim7xxx(dte.get(), voltage, bcs, bcl);
}

command_result SIM7600::set_network_bands(const std::string &mode, const int *bands, int size)
{
    return dce_commands::set_network_bands_sim76xx(dte.get(), mode, bands, size);
}

command_result SIM7600::set_gnss_power_mode(int mode)
{
    return dce_commands::set_gnss_power_mode_sim76xx(dte.get(), mode);
}

command_result SIM7600::power_down()
{
    return dce_commands::power_down_sim76xx(dte.get());
}

command_result SIM7070::power_down()
{
    return dce_commands::power_down_sim70xx(dte.get());
}

command_result SIM7070::set_data_mode()
{
    return dce_commands::set_data_mode_alt(dte.get());
}

command_result SIM7000::power_down()
{
    return dce_commands::power_down_sim70xx(dte.get());
}

command_result SIM800::power_down()
{
    return dce_commands::power_down_sim8xx(dte.get());
}

command_result BG96::set_pdp_context(esp_modem::PdpContext &pdp)
{
    return dce_commands::set_pdp_context(dte.get(), pdp, 300);
}

bool SQNGM02S::setup_data_mode()
{
    return true;
}

command_result SQNGM02S::connect(PdpContext &pdp)
{
    command_result res;
    configure_pdp_context(std::make_unique<PdpContext>(pdp));
    set_pdp_context(*this->pdp);
    res = config_network_registration_urc(1);
    if (res != command_result::OK) {
        return res;
    }

    res = set_radio_state(1);
    if (res != command_result::OK) {
        return res;
    }

    //wait for +CEREG: 5 or +CEREG: 1.
    const auto pass = std::list<std::string_view>({"+CEREG: 1", "+CEREG: 5"});
    const auto fail = std::list<std::string_view>({"ERROR"});
    res = esp_modem::dce_commands::generic_command(dte.get(), "", pass, fail, 1200000);

    if (res != command_result::OK) {
        config_network_registration_urc(0);
        return res;
    }

    res = config_network_registration_urc(0);
    if (res != command_result::OK) {
        return res;
    }

    return command_result::OK;
}
}
