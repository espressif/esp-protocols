#pragma once

#include "cxx_include/esp_modem_dce_commands.hpp"
//std::shared_ptr<DeviceIf> create_device(const std::shared_ptr<DTE>& dte, std::string &apn);
std::shared_ptr<Device> create_device(const std::shared_ptr<DTE>& dte, std::string &apn);