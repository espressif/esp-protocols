#pragma once
#include "cxx_include/esp_modem_netif.hpp"
#include "generate/esp_modem_command_declare.inc"

template<class SpecificModule>
class DCE {
    static_assert(std::is_base_of<ModuleIf, SpecificModule>::value, "DCE must be instantiated with Module class only");
public:
    explicit DCE(const std::shared_ptr<DTE>& dte, std::shared_ptr<SpecificModule> device, esp_netif_t * netif):
        dce_dte(dte), device(std::move(device)), ppp_netif(dte, netif)
    { }
    void set_data() {
        device->setup_data_mode();
        device->set_mode(modem_mode::DATA_MODE);
        dce_dte->set_mode(modem_mode::DATA_MODE);
        ppp_netif.start();
    }
    void exit_data() {
        ppp_netif.stop();
        device->set_mode(modem_mode::COMMAND_MODE);
//        uint8_t* data;
        dce_dte->set_data_cb([&](size_t len) -> bool {
//            auto actual_len = dce_dte->read(&data, 64);
//            ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", data, actual_len, ESP_LOG_INFO);
            return false;
        });
        ppp_netif.wait_until_ppp_exits();
        dce_dte->set_data_cb(nullptr);
        dce_dte->set_mode(modem_mode::COMMAND_MODE);
    }

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
    template <typename ...Agrs> \
    return_type name(Agrs&&... args)   \
    {   \
        return device->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...) { device->name(...); } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND


//    template <typename ...Params>
//    command_result get_module_name(Params&&... params)
//    {
//        return device->get_module_name(std::forward<Params>(params)...);
//    }
//    template <typename ...Params>
//    command_result set_data_mode(Params&&... params)
//    {
//        return device->set_data_mode(std::forward<Params>(params)...);
//    }


    command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms) {
        return dce_dte->command(command, got_line, time_ms);
    }
    void set_cmux() {    device->set_mode(modem_mode::CMUX_MODE);
        dce_dte->set_mode(modem_mode::CMUX_MODE); }
private:
    std::shared_ptr<DTE> dce_dte;
    std::shared_ptr<SpecificModule> device;
    PPP ppp_netif;
};