#pragma once
#include "cxx_include/esp_modem_dce_commands_if.hpp"
#include "cxx_include/esp_modem_commands.hpp"
#include <memory>
#include <utility>

enum class command_result;
class DTE;

class Device: public DeviceIf {
public:
    explicit Device(std::shared_ptr<DTE> dte, std::unique_ptr<PdpContext> pdp):
        dte(std::move(dte)), pdp(std::move(pdp)) {}
    bool setup_data_mode() override;
    bool set_mode(dte_mode mode) override;

    command_result set_echo(bool on) { return esp_modem::dce_commands::set_echo(dte, on); }
    command_result set_data_mode() { return esp_modem::dce_commands::set_data_mode(dte); }
    command_result resume_data_mode() { return esp_modem::dce_commands::resume_data_mode(dte); }
    command_result set_pdp_context(PdpContext& pdp_context) { return esp_modem::dce_commands::set_pdp_context(dte.get(), pdp_context); }
    command_result set_command_mode() { return esp_modem::dce_commands::set_command_mode(dte); }

private:
    std::shared_ptr<DTE> dte;
    std::unique_ptr<PdpContext> pdp;
};


