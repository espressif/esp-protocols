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

#pragma once

#include <memory>
#include <utility>
#include "generate/esp_modem_command_declare.inc"
#include "cxx_include/esp_modem_command_library.hpp"
#include "cxx_include/esp_modem_types.hpp"
#include "esp_modem_dce_config.h"

namespace esp_modem {

/**
 * @defgroup ESP_MODEM_MODULE
 * @brief Definition of modules representing specific modem devices
 */

/** @addtogroup ESP_MODEM_MODULE
* @{
*/

enum class command_result;
class DTE;
struct PdpContext;

/**
 * @brief This is a basic building block for custom modules as well as for the supported modules in the esp-modem component
 * It derives from the ModuleIf.
 */
class GenericModule: public ModuleIf {
public:
    /**
     * @brief We can construct a generic device with an existent DTE and it's configuration
     * The configuration could be either the dce-config struct or just a pdp context
     */
    explicit GenericModule(std::shared_ptr<DTE> dte, std::unique_ptr<PdpContext> pdp):
        dte(std::move(dte)), pdp(std::move(pdp)) {}
    explicit GenericModule(std::shared_ptr<DTE> dte, const esp_modem_dce_config *config);

    /**
     * @brief This is a mandatory method for ModuleIf class, which sets up the device
     * to be able to connect to the network. This typically consists of setting basic
     * communication parameters and setting the PDP (defining logical access point
     * to cellular network)
     */
    bool setup_data_mode() override
    {
        if (set_echo(false) != command_result::OK) {
            return false;
        }
        if (set_pdp_context(*pdp) != command_result::OK) {
            return false;
        }
        return true;
    }

    /**
     * @brief This is a mandatory method of ModuleIf class, which defines
     * basic commands for switching between DATA, COMMAND and CMUX modes
     */
    bool set_mode(modem_mode mode) override
    {
        if (mode == modem_mode::DATA_MODE) {
            if (set_data_mode() != command_result::OK) {
                return resume_data_mode() == command_result::OK;
            }
            return true;
        } else if (mode == modem_mode::COMMAND_MODE) {
            return set_command_mode() == command_result::OK;
        } else if (mode == modem_mode::CMUX_MODE) {
            return set_cmux() == command_result::OK;
        }
        return true;
    }

    /**
     * @brief Additional method providing runtime configuration of PDP context
     */
    void configure_pdp_context(std::unique_ptr<PdpContext> new_pdp)
    {
        pdp = std::move(new_pdp);
    }

    /**
     * @brief Common DCE commands generated from the API AT list
     */
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    virtual return_type name(__VA_ARGS__);

    DECLARE_ALL_COMMAND_APIS(virtual return_type name(...); )

#undef ESP_MODEM_DECLARE_DCE_COMMAND


protected:
    std::shared_ptr<DTE> dte;         /*!< Generic device needs the DTE as a channel talk to the module using AT commands */
    std::unique_ptr<PdpContext> pdp;  /*!< It also needs a PDP data, const information used for setting up cellular network */
};

// Definitions of other supported modules with some specific commands overwritten

/**
 * @brief Specific definition of the SIM7600 module
 */
class SIM7600: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result get_module_name(std::string &name) override;
    command_result get_battery_status(int &voltage, int &bcs, int &bcl) override;
    command_result power_down() override;
};

/**
 * @brief Specific definition of the SIM800 module
 */
class SIM800: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result get_module_name(std::string &name) override;
    command_result power_down() override;
    command_result set_data_mode() override;
};

/**
 * @brief Specific definition of the BG96 module
 */
class BG96: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result get_module_name(std::string &name) override;
};

/**
 * @}
 */

} // namespace esp_modem
