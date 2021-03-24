//
// Created by david on 3/24/21.
//

#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include "esp_log.h"

namespace esp_modem::DCE {


bool Modes::set(DTE *dte, ModuleIf *device, PPP &netif, modem_mode m)
{
    switch (m) {
        case modem_mode::UNDEF:
            break;
        case modem_mode::COMMAND_MODE:
            if (mode == modem_mode::COMMAND_MODE)
                return false;
            netif.stop();
            device->set_mode(modem_mode::COMMAND_MODE);
            uint8_t* data;
            dte->set_data_cb([&](size_t len) -> bool {
                auto actual_len = dte->read(&data, 64);
                ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", data, actual_len, ESP_LOG_INFO);
                return false;
            });
            netif.wait_until_ppp_exits();
            dte->set_data_cb(nullptr);
            dte->set_mode(modem_mode::COMMAND_MODE);
            mode = m;
            return true;
        case modem_mode::DATA_MODE:
            if (mode == modem_mode::DATA_MODE)
                return false;
            device->setup_data_mode();
            device->set_mode(modem_mode::DATA_MODE);
            dte->set_mode(modem_mode::DATA_MODE);
            netif.start();
            mode = m;
            return true;
        case modem_mode::CMUX_MODE:
            if (mode == modem_mode::DATA_MODE || mode == modem_mode::CMUX_MODE)
                return false;
            device->set_mode(modem_mode::CMUX_MODE);
            dte->set_mode(modem_mode::CMUX_MODE);
            mode = modem_mode::COMMAND_MODE;
            return true;
    }
    return false;
}

modem_mode Modes::get()
{
    return mode;
}

}


