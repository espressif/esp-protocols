//
// Created by david on 3/25/21.
//
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include <memory>
#include <utility>

class NetModule;
typedef DCE_T<MinimalModule> NetDCE;

class NetModule: public ModuleIf {
public:
    explicit NetModule(std::shared_ptr<DTE> dte):
            dte(std::move(dte)) {}

    bool setup_data_mode() override
    {
        set_command_mode(); // send in case we were in PPP mode, ignore potential failure
        bool is_pin_ok;
        if (read_pin(is_pin_ok) != command_result::OK)
            return false;
        if (!is_pin_ok) {
            if (set_pin(pin) != command_result::OK)
                return ESP_FAIL;
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (read_pin(is_pin_ok) != command_result::OK || !is_pin_ok)
                return ESP_FAIL;
        }

        PdpContext pdp(apn);
        if (set_pdp_context(pdp) != command_result::OK)
            return false;
        return true;
    }

    bool set_mode(modem_mode mode) override
    {
        if (mode == modem_mode::DATA_MODE) {
            if (set_data_mode() != command_result::OK)
                return resume_data_mode() == command_result::OK;
            return true;
        }
        if (mode == modem_mode::COMMAND_MODE) {
            return set_command_mode() == command_result::OK;
        }
        return false;
    }

    static esp_err_t init(esp_netif_t *netif, std::string apn_name, std::string pin_number)
    {
        apn = std::move(apn_name);
        pin = std::move(pin_number);
        esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
        dte_config.event_task_stack_size = 4096;
        dte_config.rx_buffer_size = 16384;
        dte_config.tx_buffer_size = 2048;

        // create
        auto uart_dte = create_uart_dte(&dte_config);
        esp_modem::DCE::Factory f(esp_modem::DCE::Modem::MinModule);
        MinimalModule* module;
        if (!f.build_module_T<MinimalModule>(module, uart_dte, netif)) {
            return ESP_OK;
        }
        esp_modem::DCE::Factory f2(esp_modem::DCE::Modem::SIM7600);
//        std::shared_ptr<MinimalModule> dev;
        auto dev = f2.build_shared_module(uart_dte, netif);

        //        esp_modem::DCE::Builder<MinimalModule> factory(uart_dte, netif);
//        dce = factory.create(apn_name);
//        auto pdp = std::make_unique<PdpContext>(apn);
//        auto dev = std::make_shared<MinimalModule>(uart_dte, std::move(pdp));
//        auto dev = std::make_shared<MinimalModule>(uart_dte, nullptr);
//        dce = new DCE_T<MinimalModule>(uart_dte, module, netif);
        return ESP_OK;
    }

    static void deinit()    { delete dce; }
    static void start()     { dce->set_data();  }
    static void stop()      { dce->exit_data(); }

private:
    static NetDCE *dce;
    std::shared_ptr<DTE> dte;
    static std::string apn;
    static std::string pin;

    template <typename ...T> command_result set_pdp_context(T&&... args) { return esp_modem::dce_commands::set_pdp_context(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result set_pin(T&&... args) { return esp_modem::dce_commands::set_pin(dte.get(),std::forward<T>(args)...); }
    template <typename ...T> command_result read_pin(T&&... args) { return esp_modem::dce_commands::read_pin(dte.get(),std::forward<T>(args)...); }
    command_result set_data_mode() { return esp_modem::dce_commands::set_data_mode(dte.get()); }
    command_result resume_data_mode() { return esp_modem::dce_commands::resume_data_mode(dte.get()); }
    command_result set_command_mode() { return esp_modem::dce_commands::set_command_mode(dte.get()); }
};

NetDCE *NetModule::dce = nullptr;
std::string NetModule::apn;
std::string NetModule::pin;

extern "C" esp_err_t modem_init_network(esp_netif_t *netif)
{
    return NetModule::init(netif, "internet", "1234");
}

extern "C" void modem_start_network()
{
    NetModule::start();
}

extern "C" void modem_stop_network()
{
    NetModule::stop();
}
