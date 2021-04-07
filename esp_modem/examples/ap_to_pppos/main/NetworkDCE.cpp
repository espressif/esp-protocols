//
// Created by david on 3/25/21.
//
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include <memory>
#include <utility>

using namespace esp_modem;
using namespace esp_modem::dce_factory;

class NetModule;
typedef DCE_T<NetModule> NetDCE;

class NetDCE_Factory: public Factory {
public:
    template <typename T, typename ...Args>
    static DCE_T<T>* create(const config *cfg, Args&&... args)
    {
        return build_generic_DCE<T>
                (cfg, std::forward<Args>(args)...);
    }
};

class NetModule: public ModuleIf {
public:
    explicit NetModule(std::shared_ptr<DTE> dte, const esp_modem_dce_config *cfg):
            dte(std::move(dte)), apn(std::string(cfg->apn)) {}

    bool setup_data_mode() override
    {
        // switch to command mode (in case we were in PPP mode)
        static_cast<void>(set_command_mode()); // ignore the potential failure, as we might be in command mode after startup

        bool is_pin_ok;
        if (read_pin(is_pin_ok) != command_result::OK)
            return false;
        if (!is_pin_ok) {
            if (set_pin() != command_result::OK)
                return false;
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (read_pin(is_pin_ok) != command_result::OK || !is_pin_ok)
                return false;
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

    static esp_err_t init(esp_netif_t *netif, const std::string& apn, std::string pin_number)
    {
        // configure
        pin = std::move(pin_number);
        esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
        dte_config.event_task_stack_size = 4096;
        dte_config.rx_buffer_size = 16384;
        dte_config.tx_buffer_size = 2048;
        esp_modem_dce_config dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(apn.c_str());

        // create DTE and minimal network DCE
        auto uart_dte = create_uart_dte(&dte_config);
        dce = NetDCE_Factory::create<NetModule>(&dce_config, uart_dte, netif);
        return dce == nullptr ? ESP_FAIL : ESP_OK;
    }

    static void deinit()    { delete dce; }
    static void start()     { dce->set_data();  }
    static void stop()      { dce->exit_data(); }

private:
    static NetDCE *dce;
    std::shared_ptr<DTE> dte;
    std::string apn;
    static std::string pin;

    [[nodiscard]] command_result set_pdp_context(PdpContext& pdp) { return dce_commands::set_pdp_context(dte.get(),pdp); }
    [[nodiscard]] command_result set_pin() { return dce_commands::set_pin(dte.get(), pin); }
    [[nodiscard]] command_result read_pin(bool& pin_ok) { return dce_commands::read_pin(dte.get(), pin_ok); }
    [[nodiscard]] command_result set_data_mode() { return dce_commands::set_data_mode(dte.get()); }
    [[nodiscard]] command_result resume_data_mode() { return dce_commands::resume_data_mode(dte.get()); }
    [[nodiscard]] command_result set_command_mode() { return dce_commands::set_command_mode(dte.get()); }
};

NetDCE *NetModule::dce = nullptr;
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
