#include <functional>
#include "cxx_include/esp_modem_types.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

template<typename T>
std::shared_ptr<T> create_device(const std::shared_ptr<DTE>& dte, std::string &apn)
{
    auto pdp = std::make_unique<PdpContext>(apn);
    return std::make_shared<T>(dte, std::move(pdp));
}

std::shared_ptr<GenericModule> create_generic_module(const std::shared_ptr<DTE>& dte, std::string &apn)
{
    return create_device<GenericModule>(dte, apn);
}

std::shared_ptr<SIM7600> create_SIM7600_module(const std::shared_ptr<DTE>& dte, std::string &apn)
{
    return create_device<SIM7600>(dte, apn);
}
