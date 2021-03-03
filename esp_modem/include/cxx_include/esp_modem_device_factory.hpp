#pragma once

std::shared_ptr<DeviceIf> create_device(const std::shared_ptr<DTE>& dte, std::string &apn);