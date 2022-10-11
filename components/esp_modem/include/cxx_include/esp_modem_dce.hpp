/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

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
    DCE_Mode(): mode(modem_mode::UNDEF) {}
    ~DCE_Mode() = default;
    bool set(DTE *dte, ModuleIf *module, Netif &netif, modem_mode m);
    modem_mode get();

private:
    bool set_unsafe(DTE *dte, ModuleIf *module, Netif &netif, modem_mode m);
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
    explicit DCE_T(const std::shared_ptr<DTE> &dte, std::shared_ptr<SpecificModule> dev, esp_netif_t *netif):
        dte(dte), device(std::move(dev)), netif(dte, netif)
    { }

    ~DCE_T() = default;

    /**
     * @brief Set data mode!
     */
    void set_data()
    {
        set_mode(modem_mode::DATA_MODE);
    }

    void exit_data()
    {
        set_mode(modem_mode::COMMAND_MODE);
    }

    void set_cmux()
    {
        set_mode(modem_mode::CMUX_MODE);
    }

    SpecificModule *get_module()
    {
        return device.get();
    }

    command_result command(const std::string &command, got_line_cb got_line, uint32_t time_ms)
    {
        return dte->command(command, std::move(got_line), time_ms);
    }

    bool set_mode(modem_mode m)
    {
        return mode.set(dte.get(), device.get(), netif, m);
    }

protected:
    std::shared_ptr<DTE> dte;
    std::shared_ptr<SpecificModule> device;
    Netif netif;
    DCE_Mode mode;
};

/**
 * @brief Common abstraction of the modem DCE, specialized by the GenericModule which is a parent class for the supported
 * devices and most common modems, as well.
 */
class DCE : public DCE_T<GenericModule> {
public:

    using DCE_T<GenericModule>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    template <typename ...Agrs> \
    return_type name(Agrs&&... args)   \
    {   \
        return device->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...)
    {
        device->name(...);
    } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

};

/**
 * @}
 */

} // esp_modem
