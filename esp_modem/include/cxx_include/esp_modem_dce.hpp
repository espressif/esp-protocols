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

#ifndef _ESP_MODEM_DCE_HPP_
#define _ESP_MODEM_DCE_HPP_

#include <utility>
#include "cxx_include/esp_modem_netif.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

namespace esp_modem {

/**
 * @defgroup ESP_MODEM_DCE
 * @brief Definition of DCE abstraction
 */
/** @addtogroup ESP_MODEM_DCE
 * @{
 */


/**
 * @brief Helper class responsible for switching modes of the DCE's
 */
class DCE_Mode {
public:
    DCE_Mode(): mode(modem_mode::COMMAND_MODE) {}
    ~DCE_Mode() = default;
    bool set(DTE *dte, ModuleIf *module, Netif &netif, modem_mode m);
    modem_mode get();

private:
    modem_mode mode;

};

/**
 * @brief General DCE class templated on a specific module. It is responsible for all the necessary transactions
 * related to switching modes and consequent synergy with aggregated objects of DTE, Netif and a specific Module
 */
template<class SpecificModule>
class DCE_T {
    static_assert(std::is_base_of<ModuleIf, SpecificModule>::value, "DCE must be instantiated with Module class only");
public:
    explicit DCE_T(const std::shared_ptr<DTE>& dte, std::shared_ptr<SpecificModule> device, esp_netif_t * netif):
            dte(dte), module(std::move(device)), netif(dte, netif)
    { }

    ~DCE_T() = default;

    /**
     * @brief Set data mode!
     */
    void set_data() { set_mode(modem_mode::DATA_MODE); }

    void exit_data() { set_mode(modem_mode::COMMAND_MODE); }

    void set_cmux() { set_mode(modem_mode::CMUX_MODE); }

    ModuleIf* get_module() { return module.get(); }


    command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms)
    {
        return dte->command(command, std::move(got_line), time_ms);
    }

    bool set_mode(modem_mode m) { return mode.set(dte.get(), module.get(), netif, m); }

protected:


    std::shared_ptr<DTE> dte;
    std::shared_ptr<SpecificModule> module;
    Netif netif;
    DCE_Mode mode;
};

/**
 * @brief Common abstraction of the modem DCE, specialized by the GenericModule which is a parent class for the supported
 * defices and most common modems, as well.
 */
class DCE: public DCE_T<GenericModule> {
public:

    using DCE_T<GenericModule>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    template <typename ...Agrs> \
    return_type name(Agrs&&... args)   \
    {   \
        return module->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...) { device->name(...); } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

};

/**
 * @}
 */

} // esp_modem

#endif // _ESP_MODEM_DCE_HPP_