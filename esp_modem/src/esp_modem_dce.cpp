#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include <cstring>

#include <utility>
#include "esp_log.h"

DCE::DCE(const std::shared_ptr<DTE>& dte, std::shared_ptr<DeviceIf> device, esp_netif_t * netif):
        dce_dte(dte), device(std::move(device)), ppp_netif(dte, netif)
{ }

void DCE::exit_data() {
    ppp_netif.stop();
    device->set_mode(dte_mode::COMMAND_MODE);
    uint8_t* data;
    dce_dte->set_data_cb([&](size_t len) -> bool {
        auto actual_len = dce_dte->read(&data, 64);
        ESP_LOG_BUFFER_HEXDUMP("esp-modem: debug_data", data, actual_len, ESP_LOG_INFO);
        return false;
    });
    ppp_netif.wait_until_ppp_exits();
    dce_dte->set_data_cb(nullptr);
    dce_dte->set_mode(dte_mode::COMMAND_MODE);
}

void DCE::set_cmux()
{
    device->set_mode(dte_mode::CMUX_MODE);
    dce_dte->set_mode(dte_mode::CMUX_MODE);
//    auto t = dce_dte->detach();
//    auto cmux = std::make_unique<CMUXedTerminal>(std::move(t), dce_dte->get_buffer());
//    dce_dte->attach(std::move(cmux));
}