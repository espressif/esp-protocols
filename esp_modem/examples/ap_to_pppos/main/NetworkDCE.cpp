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

/**
 * @brief Local network object used to setup PPP network
 */
class PPPNetwork {
public:
    esp_err_t init(esp_netif_t *netif, const std::string& apn, const std::string &pin_number);
    NetDCE * get_dce();
private:
    NetDCE *dce;
};

/**
 * @brief The PPP network is a singleton, allocate statically here
 */
static PPPNetwork ppp_network;

/**
 * @brief Custom factory for creating NetDCE and NetModule
 */
class NetDCE_Factory: public Factory {
public:
    template <typename T, typename ...Args>
    static DCE_T<T>* create(const config *cfg, Args&&... args)
    {
        return build_generic_DCE<T>(cfg, std::forward<Args>(args)...);
    }

    template <typename T, typename ...Args>
    static std::shared_ptr<T> create_module(const config *cfg, Args&&... args)
    {
        return build_shared_module<T>(cfg, std::forward<Args>(args)...);
    }
};

/**
 * @brief This is an example of implementing minimal network module functionality
 *
 * This does only include those AT commands, that are needed for setting the network up
 * and also initialization part (set pin, ...)
 */
class NetModule: public ModuleIf {
public:
    explicit NetModule(std::shared_ptr<DTE> dte, const esp_modem_dce_config *cfg):
            dte(std::move(dte)), apn(std::string(cfg->apn)) {}

    bool setup_data_mode() override
    {
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

    bool init(const std::string& pin)
    {
        // switch to command mode (in case we were in PPP mode)
        static_cast<void>(set_command_mode()); // ignore the potential failure, as we might be in command mode after startup
        bool is_pin_ok;
        if (read_pin(is_pin_ok) != command_result::OK)
            return false;
        if (!is_pin_ok) {
            if (set_pin(pin) != command_result::OK)
                return false;
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (read_pin(is_pin_ok) != command_result::OK || !is_pin_ok)
                return false;
        }
        return true;
    }

private:
    std::shared_ptr<DTE> dte;
    std::string apn;

    [[nodiscard]] command_result set_pdp_context(PdpContext& pdp) { return dce_commands::set_pdp_context(dte.get(),pdp); }
    [[nodiscard]] command_result set_pin(const std::string &pin) { return dce_commands::set_pin(dte.get(), pin); }
    [[nodiscard]] command_result read_pin(bool& pin_ok) { return dce_commands::read_pin(dte.get(), pin_ok); }
    [[nodiscard]] command_result set_data_mode() { return dce_commands::set_data_mode(dte.get()); }
    [[nodiscard]] command_result resume_data_mode() { return dce_commands::resume_data_mode(dte.get()); }
    [[nodiscard]] command_result set_command_mode() { return dce_commands::set_command_mode(dte.get()); }
};


esp_err_t PPPNetwork::init(esp_netif_t *netif, const std::string& apn, const std::string &pin_number)
{
    // configure
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.event_task_stack_size = 4096;
    dte_config.rx_buffer_size = 16384;
    dte_config.tx_buffer_size = 2048;
    esp_modem_dce_config dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(apn.c_str());

    // create DTE and minimal network DCE
    auto uart_dte = create_uart_dte(&dte_config);

    // create the specific device (and initialize it)
    auto dev = NetDCE_Factory::create_module<NetModule>(&dce_config, uart_dte, netif);
    if (!dev->init(pin_number))
        return ESP_FAIL;

    // now create the DCE from our already existent device
    dce = NetDCE_Factory::create<NetModule>(&dce_config, uart_dte, netif, dev);
    if (dce == nullptr)
        return ESP_FAIL;
    return ESP_OK;
}

NetDCE *PPPNetwork::get_dce()
{
    return dce;
}

extern "C" esp_err_t modem_init_network(esp_netif_t *netif)
{
    return ppp_network.init(netif, "internet", "1234");
}

extern "C" void modem_start_network()
{
    ppp_network.get_dce()->set_mode(esp_modem::modem_mode::DATA_MODE);
}

extern "C" void modem_stop_network()
{
    ppp_network.get_dce()->set_mode(esp_modem::modem_mode::COMMAND_MODE);
}
