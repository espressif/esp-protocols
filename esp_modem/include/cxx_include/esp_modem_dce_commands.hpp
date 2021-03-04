#pragma once
#include "cxx_include/esp_modem_dce_commands_if.hpp"
#include <memory>
#include <utility>

struct PdpContext {
    PdpContext(std::string& apn): context_id(1), protocol_type("IP"), apn(apn) {}
    size_t context_id;
    std::string protocol_type;
    std::string apn;
};

enum class command_result;
class DTE;

class Device: public DeviceIf {
public:
    explicit Device(std::shared_ptr<DTE> dte, std::unique_ptr<PdpContext> pdp):
        dte(std::move(dte)), pdp(std::move(pdp)) {}
    bool setup_data_mode() override;
    bool set_mode(dte_mode mode) override;

    command_result set_echo(bool on);
    command_result set_data_mode();
    command_result resume_data_mode();
    command_result set_pdp_context(PdpContext& pdp_context);
    command_result set_command_mode();
    command_result set_cmux();
    command_result get_imsi(std::string& imsi_number);
    command_result set_pin(const std::string& pin);
    command_result read_pin(bool& pin_ok);
    command_result get_imei(std::string& imei);
    command_result get_module_name(std::string& imei);

private:
    std::shared_ptr<DTE> dte;
    std::unique_ptr<PdpContext> pdp;
};


