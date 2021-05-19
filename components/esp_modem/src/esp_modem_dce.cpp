// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include "esp_log.h"

namespace esp_modem {


bool DCE_Mode::set(DTE *dte, ModuleIf *device, Netif &netif, modem_mode m)
{
    switch (m) {
        case modem_mode::UNDEF:
            break;
        case modem_mode::COMMAND_MODE:
            if (mode == modem_mode::COMMAND_MODE)
                return false;
            netif.stop();
            if (!device->set_mode(modem_mode::COMMAND_MODE))
                return false;
            dte->set_read_cb([&](uint8_t *data, size_t len) -> bool {
                ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", data, len, ESP_LOG_INFO);
                return false;
            });
            netif.wait_until_ppp_exits();
            dte->set_read_cb(nullptr);
            if (!dte->set_mode(modem_mode::COMMAND_MODE))
                return false;
            mode = m;
            return true;
        case modem_mode::DATA_MODE:
            if (mode == modem_mode::DATA_MODE)
                return false;
            if (!device->setup_data_mode())
                return false;
            if (!device->set_mode(modem_mode::DATA_MODE))
                return false;
            if (!dte->set_mode(modem_mode::DATA_MODE))
                return false;
            netif.start();
            mode = m;
            return true;
        case modem_mode::CMUX_MODE:
            if (mode == modem_mode::DATA_MODE || mode == modem_mode::CMUX_MODE)
                return false;
            device->set_mode(modem_mode::CMUX_MODE);
            if (!dte->set_mode(modem_mode::CMUX_MODE))
                return false;
            mode = modem_mode::COMMAND_MODE;
            return true;
    }
    return false;
}

modem_mode DCE_Mode::get()
{
    return mode;
}

} // esp_modem