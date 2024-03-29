/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
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
 * @brief Custom factory which can build and create a DCE using a custom module
 */
class NetDCE_Factory: public Factory {
public:
    template <typename T, typename ...Args>
    static DCE_T<T> *create(const config *cfg, Args &&... args)
    {
        return build_generic_DCE<T>(cfg, std::forward<Args>(args)...);
    }
};

/**
 * @brief This is a null-module, doesn't define any AT commands, just passes everything to pppd
 */
class NetModule: public ModuleIf {
public:
    explicit NetModule(std::shared_ptr<DTE> dte, const esp_modem_dce_config *cfg):
        dte(std::move(dte)) {}

    bool setup_data_mode() override
    {
        return true;
    }

    bool set_mode(modem_mode mode) override
    {
        return true;
    }

    static esp_err_t init(esp_netif_t *netif)
    {
        // configure
        esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
        dte_config.uart_config.baud_rate = 921600;  // check also 460800
        esp_modem_dce_config dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("");

        // create DTE and minimal network DCE
        auto uart_dte = create_uart_dte(&dte_config);
        dce = NetDCE_Factory::create<NetModule>(&dce_config, uart_dte, netif);
        return dce == nullptr ? ESP_FAIL : ESP_OK;
    }

    static void deinit()
    {
        delete dce;
    }
    static void start()
    {
        dce->set_data();
    }
    static void stop()
    {
        dce->exit_data();
    }

private:
    static NetDCE *dce;
    std::shared_ptr<DTE> dte;
};

NetDCE *NetModule::dce = nullptr;

extern "C" esp_err_t modem_init_network(esp_netif_t *netif)
{
    return NetModule::init(netif);
}

extern "C" esp_err_t modem_start_network()
{
    NetModule::start();
    return ESP_OK;
}

extern "C" void modem_stop_network()
{
    NetModule::stop();
}
