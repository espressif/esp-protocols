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

#include <list>
#include <unistd.h>
#include <cstring>

#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include "esp_log.h"

namespace esp_modem {

/**
 * Set mode while the entire DTE is locked
*/
bool DCE_Mode::set(DTE *dte, ModuleIf *device, Netif &netif, modem_mode m)
{
    Scoped<DTE> lock(*dte);
    return set_unsafe(dte, device, netif, m);
}

/**
 * state machine:
 *
 * COMMAND_MODE <----> DATA_MODE
 * COMMAND_MODE <----> CMUX_MODE
 *
 * UNDEF <----> any
 */
bool DCE_Mode::set_unsafe(DTE *dte, ModuleIf *device, Netif &netif, modem_mode m)
{
    switch (m) {
    case modem_mode::UNDEF:
        break;
    case modem_mode::COMMAND_MODE:
        {
            if (mode == modem_mode::COMMAND_MODE) {
                return false;
            }
            if (mode == modem_mode::CMUX_MODE) {
                netif.stop();
                netif.wait_until_ppp_exits();
                if (!dte->set_mode(modem_mode::COMMAND_MODE)) {
                    return false;
                }
                mode = m;
                return true;
            }
            netif.stop();
            auto signal = std::make_shared<SignalGroup>();
            std::weak_ptr<SignalGroup> weak_signal = signal;
            dte->set_read_cb([weak_signal](uint8_t *data, size_t len) -> bool {
                if (memchr(data, '\n', len)) {
                    ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", data, len, ESP_LOG_DEBUG);
                    const auto pass = std::list<std::string_view>({"NO CARRIER", "DISCONNECTED"});
                    std::string_view response((char *) data, len);
                    for (auto &it : pass)
                        if (response.find(it) != std::string::npos) {
                            if (auto signal = weak_signal.lock())
                                signal->set(1);
                            return true;
                        }
                }
                return false;
            });
            netif.wait_until_ppp_exits();
            if (!signal->wait(1, 2000)) {
                if (!device->set_mode(modem_mode::COMMAND_MODE)) {
                    mode = modem_mode::UNDEF;
                    return false;
                }
            }
            dte->set_read_cb(nullptr);
            if (!dte->set_mode(modem_mode::COMMAND_MODE)) {
                mode = modem_mode::UNDEF;
                return false;
            }
            mode = m;
            return true;
        }
        break;
    case modem_mode::DATA_MODE:
        if (mode == modem_mode::DATA_MODE || mode == modem_mode::CMUX_MODE) {
            return false;
        }
        if (!device->setup_data_mode()) {
            return false;
        }
        if (!device->set_mode(modem_mode::DATA_MODE)) {
            return false;
        }
        if (!dte->set_mode(modem_mode::DATA_MODE)) {
            return false;
        }
        netif.start();
        mode = m;
        return true;
    case modem_mode::CMUX_MODE:
        if (mode == modem_mode::DATA_MODE || mode == modem_mode::CMUX_MODE) {
            return false;
        }
        device->set_mode(modem_mode::CMUX_MODE);    // switch the device into CMUX mode
        usleep(100'000);                            // some devices need a few ms to switch

        if (!dte->set_mode(modem_mode::CMUX_MODE)) {
            return false;
        }
        mode = modem_mode::CMUX_MODE;
        if (!device->setup_data_mode()) {
            return false;
        }
        if (!device->set_mode(modem_mode::DATA_MODE)) {
            return false;
        }
        if (!dte->set_mode(modem_mode::DATA_MODE)) {
            return false;
        }
        netif.start();
        return true;
    }
    return false;
}

modem_mode DCE_Mode::get()
{
    return mode;
}

} // esp_modem