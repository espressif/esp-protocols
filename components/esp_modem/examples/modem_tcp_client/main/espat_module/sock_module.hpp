/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"

#pragma  once

namespace sock_dce {

class Module: public esp_modem::GenericModule {
    using esp_modem::GenericModule::GenericModule;
public:

    esp_modem::command_result sync() override;
    esp_modem::command_result set_echo(bool on) override;
    esp_modem::command_result set_pdp_context(esp_modem::PdpContext &pdp) override;

};

}
