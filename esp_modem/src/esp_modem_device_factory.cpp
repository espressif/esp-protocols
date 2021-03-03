#include "cxx_include/esp_modem_dce_commands.hpp"
#include "cxx_include/esp_modem_dte.hpp"

std::shared_ptr<DeviceIf> create_device(const std::shared_ptr<DTE>& dte, std::string &apn)
{
    auto pdp = std::make_unique<PdpContext>(apn);
    return std::make_shared<Device>(dte, std::move(pdp));
}

