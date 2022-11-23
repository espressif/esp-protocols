/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <list>
#include <unistd.h>
#include <cstring>

#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include "esp_log.h"

namespace esp_modem {

namespace transitions {

static bool exit_data(DTE &dte, ModuleIf &device, Netif &netif)
{
    netif.stop();
    auto signal = std::make_shared<SignalGroup>();
    std::weak_ptr<SignalGroup> weak_signal = signal;
    dte.set_read_cb([weak_signal](uint8_t *data, size_t len) -> bool {
        if (memchr(data, '\n', len))
        {
            ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", data, len, ESP_LOG_DEBUG);
            const auto pass = std::list<std::string_view>({"NO CARRIER", "DISCONNECTED"});
            std::string_view response((char *) data, len);
            for (auto &it : pass)
                if (response.find(it) != std::string::npos) {
                    if (auto signal = weak_signal.lock()) {
                        signal->set(1);
                    }
                    return true;
                }
        }
        return false;
    });
    netif.wait_until_ppp_exits();
    if (!signal->wait(1, 2000)) {
        if (!device.set_mode(modem_mode::COMMAND_MODE)) {
            return false;
        }
    }
    dte.set_read_cb(nullptr);
    if (!dte.set_mode(modem_mode::COMMAND_MODE)) {
        return false;
    }
    return true;
}

static bool enter_data(DTE &dte, ModuleIf &device, Netif &netif)
{
    if (!device.setup_data_mode()) {
        return false;
    }
    if (!device.set_mode(modem_mode::DATA_MODE)) {
        return false;
    }
    if (!dte.set_mode(modem_mode::DATA_MODE)) {
        return false;
    }
    netif.start();
    return true;
}

} // namespace transitions

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
        if (mode == modem_mode::COMMAND_MODE || mode >= modem_mode::CMUX_MANUAL_MODE) {
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
        if (!transitions::exit_data(*dte, *device, netif)) {
            mode = modem_mode::UNDEF;
            return false;
        }
        mode = m;
        return true;
    case modem_mode::DATA_MODE:
        if (mode == modem_mode::DATA_MODE || mode == modem_mode::CMUX_MODE || mode >= modem_mode::CMUX_MANUAL_MODE) {
            return false;
        }
        if (!transitions::enter_data(*dte, *device, netif)) {
            return false;
        }
        mode = m;
        return true;
    case modem_mode::CMUX_MODE:
        if (mode == modem_mode::DATA_MODE || mode == modem_mode::CMUX_MODE || mode >= modem_mode::CMUX_MANUAL_MODE) {
            return false;
        }
        device->set_mode(modem_mode::CMUX_MODE);    // switch the device into CMUX mode
        usleep(100'000);                            // some devices need a few ms to switch

        if (!dte->set_mode(modem_mode::CMUX_MODE)) {
            return false;
        }
        mode = modem_mode::CMUX_MODE;
        return transitions::enter_data(*dte, *device, netif);
    case modem_mode::CMUX_MANUAL_MODE:
        if (mode != modem_mode::COMMAND_MODE && mode != modem_mode::UNDEF) {
            return false;
        }
        device->set_mode(modem_mode::CMUX_MODE);
        usleep(100'000);

        if (!dte->set_mode(m)) {
            return false;
        }
        mode = modem_mode::CMUX_MANUAL_MODE;
        return true;
    case modem_mode::CMUX_MANUAL_EXIT:
        if (mode != modem_mode::CMUX_MANUAL_MODE) {
            return false;
        }
        if (!dte->set_mode(m)) {
            return false;
        }
        mode = modem_mode::COMMAND_MODE;
        return true;
    case modem_mode::CMUX_MANUAL_SWAP:
        if (mode != modem_mode::CMUX_MANUAL_MODE) {
            return false;
        }
        if (!dte->set_mode(m)) {
            return false;
        }
        return true;
    case modem_mode::CMUX_MANUAL_DATA:
        if (mode != modem_mode::CMUX_MANUAL_MODE) {
            return false;
        }
        return transitions::enter_data(*dte, *device, netif);
    case modem_mode::CMUX_MANUAL_COMMAND:
        if (mode != modem_mode::CMUX_MANUAL_MODE) {
            return false;
        }
        return transitions::exit_data(*dte, *device, netif);
    }
    return false;
}

modem_mode DCE_Mode::get()
{
    return mode;
}

} // esp_modem
