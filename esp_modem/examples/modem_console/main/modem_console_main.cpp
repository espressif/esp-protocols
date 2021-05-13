/* Modem console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "esp_log.h"
#include "console_helper.hpp"

extern "C" void modem_console_register_http(void);
extern "C" void modem_console_register_ping(void);

static const char *TAG = "modem_console";

static esp_console_repl_t *s_repl = nullptr;

using namespace esp_modem;

#define CHECK_ERR(cmd, success_action)  do { \
        auto err = cmd; \
        if (err == command_result::OK) {     \
            success_action;                  \
            return 0;                        \
        } else {    \
            ESP_LOGE(TAG, "Failed with %s", err == command_result::TIMEOUT ? "TIMEOUT":"ERROR");  \
        return 1;                            \
        } } while (0)

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // init the DTE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    esp_modem_dte_config_t dte_config2 = {
            .dte_buffer_size = 512,
            .vfs_config = {.port_num = UART_NUM_1,
                    .dev_name = "/dev/uart/1",
                    .rx_buffer_size = 1024,
                    .tx_buffer_size = 1024,
                    .baud_rate = 115200,
                    .tx_io_num = 25,
                    .rx_io_num = 26,
                    .task_stack_size = 4096,
                    .task_prio = 5}
    };

    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();

    esp_netif_t *esp_netif = esp_netif_new(&ppp_netif_config);
    assert(esp_netif);
    auto uart_dte = create_vfs_dte(&dte_config2);
//    auto uart_dte = create_uart_dte(&dte_config);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");

    auto dce = create_SIM7600_dce(&dce_config, uart_dte, esp_netif);
    assert(dce != nullptr);

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    // init console REPL environment
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &s_repl));

    modem_console_register_http();
    modem_console_register_ping();
    const struct SetModeArgs {
        SetModeArgs(): mode(STR1, nullptr, nullptr, "<mode>", "PPP or CMD") {}
        CommandArgs mode;
    } set_mode_args;
    const ConsoleCommand SetModeParser("set_mode", "sets modem mode", &set_mode_args, sizeof(set_mode_args), [&](ConsoleCommand *c){
        if (c->get_count_of(&SetModeArgs::mode)) {
            auto mode = c->get_string_of(&SetModeArgs::mode);
            if (mode == "CMD") {
                ESP_LOGI(TAG, "Switching to command mode...");
                dce->exit_data();
            } else if (mode == "PPP") {
                ESP_LOGI(TAG, "Switching to data mode...");
                dce->set_data();
            } else {
                ESP_LOGE(TAG, "Unsupported mode: %s", mode.c_str());
                return 1;
            }
        }
        return 0;
    });

    const struct SetPinArgs {
        SetPinArgs(): pin(STR1, nullptr, nullptr, "<pin>", "PIN") {}
        CommandArgs pin;
    } set_pin_args;
    const ConsoleCommand SetPinParser("set_pin", "sets SIM card PIN", &set_pin_args, sizeof(set_pin_args), [&](ConsoleCommand *c){
        if (c->get_count_of(&SetPinArgs::pin)) {
            auto pin = c->get_string_of(&SetPinArgs::pin);
            ESP_LOGI(TAG, "Setting pin=%s...", pin.c_str());
            auto err = dce->set_pin(pin);
            if (err == command_result::OK) {
                ESP_LOGI(TAG, "OK");
            } else {
                ESP_LOGE(TAG, "Failed %s", err == command_result::TIMEOUT ? "TIMEOUT":"");
                return 1;
            }
        }
        return 0;
    });

    const std::vector<CommandArgs> no_args;
    const ConsoleCommand ReadPinArgs("read_pin", "checks if SIM is unlocked", no_args, [&](ConsoleCommand *c){
        bool pin_ok;
        ESP_LOGI(TAG, "Checking pin...");
        auto err = dce->read_pin(pin_ok);
        if (err == command_result::OK) {
            ESP_LOGI(TAG, "OK. Pin status: %s", pin_ok ? "true": "false");
        } else {
            ESP_LOGE(TAG, "Failed %s", err == command_result::TIMEOUT ? "TIMEOUT":"");
            return 1;
        }
        return 0;
    });

    const ConsoleCommand GetModuleName("get_module_name", "reads the module name", no_args, [&](ConsoleCommand *c){
        std::string module_name;
        ESP_LOGI(TAG, "Reading module name...");
        CHECK_ERR(dce->get_module_name(module_name), ESP_LOGI(TAG, "OK. Module name: %s", module_name.c_str()));
    });

    const ConsoleCommand GetOperatorName("get_operator_name", "reads the operator name", no_args, [&](ConsoleCommand *c){
        std::string operator_name;
        ESP_LOGI(TAG, "Reading operator name...");
        CHECK_ERR(dce->get_operator_name(operator_name), ESP_LOGI(TAG, "OK. Operator name: %s", operator_name.c_str()));
    });

    const struct GenericCommandArgs {
        GenericCommandArgs():
            cmd(STR1, nullptr, nullptr, "<command>", "AT command to send to the modem"),
            timeout(INT0, "t", "timeout", "<timeout>", "command timeout"),
            pattern(STR0, "p", "pattern", "<pattern>", "command response to wait for"),
            no_cr(LIT0, "n", "no-cr", "not add trailing CR to the command") {}
        CommandArgs cmd;
        CommandArgs timeout;
        CommandArgs pattern;
        CommandArgs no_cr;
    } send_cmd_args;
    const ConsoleCommand SendCommand("cmd", "sends generic AT command, no_args", &send_cmd_args, sizeof(send_cmd_args), [&](ConsoleCommand *c) {
        auto cmd = c->get_string_of(&GenericCommandArgs::cmd);
        auto timeout = c->get_count_of(&GenericCommandArgs::timeout) ? c->get_int_of(&GenericCommandArgs::timeout)
                                                                     : 1000;
        auto pattern = c->get_string_of(&GenericCommandArgs::pattern);
        if (c->get_count_of(&GenericCommandArgs::no_cr) == 0) {
            cmd += '\r';
        }
        ESP_LOGI(TAG, "Sending command %s with timeout %d", cmd.c_str(), timeout);
        CHECK_ERR(dce->command(cmd, [&](uint8_t *data, size_t len) {
            std::string response((char *) data, len);
            ESP_LOGI(TAG, "%s", response.c_str());
            if (pattern.empty() || response.find(pattern) != std::string::npos)
                return command_result::OK;
            if (response.find(pattern) != std::string::npos)
                return command_result::OK;
            return command_result::TIMEOUT;
        }, timeout),);
    });

    const ConsoleCommand GetSignalQuality("get_signal_quality", "Gets signal quality", no_args, [&](ConsoleCommand *c){
        int rssi, ber;
        CHECK_ERR(dce->get_signal_quality(rssi, ber), ESP_LOGI(TAG, "OK. rssi=%d, ber=%d", rssi, ber));
    });
    const ConsoleCommand GetBatteryStatus("get_battery_status", "Reads voltage/battery status", no_args, [&](ConsoleCommand *c){
        int volt, bcl, bcs;
        CHECK_ERR(dce->get_battery_status(volt, bcl, bcs), ESP_LOGI(TAG, "OK. volt=%d, bcl=%d, bcs=%d", volt, bcl, bcs));
    });
    const ConsoleCommand PowerDown("power_down", "power down the module", no_args, [&](ConsoleCommand *c){
        ESP_LOGI(TAG, "Power down the module...");
        CHECK_ERR(dce->power_down(), ESP_LOGI(TAG, "OK"));
    });
    const ConsoleCommand Reset("reset", "reset the module", no_args, [&](ConsoleCommand *c){
        ESP_LOGI(TAG, "Resetting the module...");
        CHECK_ERR(dce->reset(), ESP_LOGI(TAG, "OK"));
    });

    SignalGroup exit_signal;
    const ConsoleCommand ExitConsole("exit", "exit the console application", no_args, [&](ConsoleCommand *c){
        ESP_LOGI(TAG, "Exiting...");
        exit_signal.set(1);
        s_repl->del(s_repl);
        return 0;
    });
    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
    ESP_LOGI(TAG, "Exiting...%d", esp_get_free_heap_size());
    // wait till for exit
    exit_signal.wait_any(1, UINT32_MAX);
}
