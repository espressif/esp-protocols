#pragma once
#include <utility>

#include "cxx_include/esp_modem_netif.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "generate/esp_modem_command_declare.inc"

namespace esp_modem::DCE {

class Modes {
public:
    Modes(): mode(modem_mode::COMMAND_MODE) {}
    ~Modes() = default;
    bool set(DTE *dte, ModuleIf *module, PPP &netif, modem_mode m);
    modem_mode get();

private:
    modem_mode mode;

};
}

template<class SpecificModule>
class DCE_T {
    static_assert(std::is_base_of<ModuleIf, SpecificModule>::value, "DCE must be instantiated with Module class only");
public:
    explicit DCE_T(const std::shared_ptr<DTE>& dte, std::shared_ptr<SpecificModule> device, esp_netif_t * netif):
            dte(dte), module(std::move(device)), netif(dte, netif)
    { }

    ~DCE_T() = default;

    void set_data() { set_mode(modem_mode::DATA_MODE); }

    void exit_data() { set_mode(modem_mode::COMMAND_MODE); }

    void set_cmux() { set_mode(modem_mode::CMUX_MODE); }


    command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms)
    {
        return dte->command(command, std::move(got_line), time_ms);
    }

protected:
    bool set_mode(modem_mode m) { return mode.set(dte.get(), module.get(), netif, m); }


    std::shared_ptr<DTE> dte;
    std::shared_ptr<SpecificModule> module;
    PPP netif;
    esp_modem::DCE::Modes mode;
};



class DCE: public DCE_T<GenericModule> {
public:

    using DCE_T<GenericModule>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
    template <typename ...Agrs> \
    return_type name(Agrs&&... args)   \
    {   \
        return module->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...) { device->name(...); } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

};