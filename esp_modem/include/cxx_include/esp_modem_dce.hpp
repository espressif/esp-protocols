#pragma once
#include "cxx_include/esp_modem_dce_commands_if.hpp"

class DCE {
public:
    explicit DCE(const std::shared_ptr<DTE>& d, std::shared_ptr<DeviceIf> device, esp_netif_t * netif);
    void set_data() {
//        command("AT+CGDCONT=1,\"IP\",\"internet\"\r", [&](uint8_t *data, size_t len) {
//            return command_result::OK;
//        }, 1000);
//        command("ATD*99***1#\r", [&](uint8_t *data, size_t len) {
//            return command_result::OK;
//        }, 1000);
        device->setup_data_mode();
        device->set_mode(dte_mode::DATA_MODE);
        dce_dte->set_mode(dte_mode::DATA_MODE);
        ppp_netif.start();
    }
    void exit_data() {
        ppp_netif.stop();
        device->set_mode(dte_mode::COMMAND_MODE);
        ppp_netif.wait_until_ppp_exits();
        dce_dte->set_mode(dte_mode::COMMAND_MODE);
    }
    command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms) {
        return dce_dte->command(command, got_line, time_ms);
    }
private:
    std::shared_ptr<DTE> dce_dte;
    std::shared_ptr<DeviceIf> device;
    ppp ppp_netif;
};