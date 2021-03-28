#pragma once
#include <memory>
#include <utility>
#include "generate/esp_modem_command_declare.inc"
#include "cxx_include/esp_modem_command_library.hpp"
#include "cxx_include/esp_modem_types.hpp"

enum class command_result;
class DTE;

class MinimalModule: public ModuleIf {
public:
    explicit MinimalModule(std::shared_ptr<DTE> dte, std::unique_ptr<PdpContext> pdp):
            dte(std::move(dte)), pdp(std::move(pdp)) {}

    bool setup_data_mode() override
    {
        if (set_echo(false) != command_result::OK)
            return false;
        if (set_pdp_context(*pdp) != command_result::OK)
            return false;
        return true;
    }

    bool set_mode(modem_mode mode) override
    {
        if (mode == modem_mode::DATA_MODE) {
            if (set_data_mode() != command_result::OK)
                return resume_data_mode() == command_result::OK;
            return true;
        } else if (mode == modem_mode::COMMAND_MODE) {
            return set_command_mode() == command_result::OK;
        } else if (mode == modem_mode::CMUX_MODE) {
            return set_cmux() == command_result::OK;
        }
        return true;
    }

    template <typename ...T> command_result set_echo(T&&... args) { return esp_modem::dce_commands::set_echo(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result set_pdp_context(T&&... args) { return esp_modem::dce_commands::set_pdp_context(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result set_data_mode(T&&... args) { return esp_modem::dce_commands::set_data_mode(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result resume_data_mode(T&&... args) { return esp_modem::dce_commands::resume_data_mode(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result set_command_mode(T&&... args) { return esp_modem::dce_commands::set_command_mode(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result set_cmux(T&&... args) { return esp_modem::dce_commands::set_cmux(dte.get(),std::forward<T>(args)...); }

protected:
    std::shared_ptr<DTE> dte;
    std::unique_ptr<PdpContext> pdp;
};

class GenericModule: public MinimalModule {
    using MinimalModule::MinimalModule;
public:

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
    virtual return_type name(__VA_ARGS__);

    DECLARE_ALL_COMMAND_APIS(virtual return_type name(...);)

#undef ESP_MODEM_DECLARE_DCE_COMMAND
};


// Definitions of other modules
class SIM7600: public GenericModule {
    using GenericModule::GenericModule;
public:
//    command_result get_module_name(std::string& name) override;
};

class SIM800: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result get_module_name(std::string& name) override;
};

class BG96: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result get_module_name(std::string& name) override;
};
