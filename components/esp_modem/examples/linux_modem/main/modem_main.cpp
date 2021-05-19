#include <memory>
#include <cassert>
#include <unistd.h>
#include <esp_log.h>
#include "cxx_include/esp_modem_terminal.hpp"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "esp_netif.h"


#define CONFIG_EXAMPLE_SIM_PIN "1234"

using namespace esp_modem;

[[maybe_unused]] static const char *TAG = "linux_modem_main";


int main()
{

    // init the DTE
    esp_modem_dte_config_t dte_config = {
            .dte_buffer_size = 512,
            .task_stack_size = 1024,
            .task_priority = 10,
            .uart_config = { },
            .vfs_config = { }
    };
    dte_config.vfs_config.dev_name = "/dev/ttyUSB0";
    dte_config.vfs_config.resource = ESP_MODEM_VFS_IS_UART; // This tells the VFS to init the UART (use termux to setup baudrate, etc.)

    esp_netif_config_t netif_config = {
            .dev_name = "/dev/net/tun",
            .if_name = "tun0"
    };
    esp_netif_t *tun_netif = esp_netif_new(&netif_config);
    auto uart_dte = create_vfs_dte(&dte_config);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");

    auto dce = create_SIM7600_dce(&dce_config, uart_dte, tun_netif);
    assert(dce != nullptr);

    dce->set_command_mode();

    bool pin_ok = true;
    if (dce->read_pin(pin_ok) == command_result::OK && !pin_ok) {
        throw_if_false(dce->set_pin(CONFIG_EXAMPLE_SIM_PIN) == command_result::OK, "Cannot set PIN!");
        usleep(1000000);
    }
    std::string str;
    dce->set_mode(esp_modem::modem_mode::CMUX_MODE);
    dce->get_imsi(str);
    ESP_LOGI(TAG, "Modem IMSI number: %s",str.c_str());
    dce->get_imei(str);
    ESP_LOGI(TAG, "Modem IMEI number: %s",str.c_str());
    dce->get_operator_name(str);
    ESP_LOGI(TAG, "Operator name: %s",str.c_str());

    dce->set_mode(esp_modem::modem_mode::DATA_MODE);

    usleep(100'000'000);
    esp_netif_destroy(tun_netif);
}