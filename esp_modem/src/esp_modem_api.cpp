//
// Created by david on 3/3/21.
//
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_terminal.hpp"
#include "esp_log.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include "esp_modem_api.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"

struct PdpContext;
using namespace esp_modem;

static const char *TAG = "modem_api";

std::shared_ptr<DTE> create_uart_dte(const esp_modem_dte_config *config)
{
    TRY_CATCH_RET_NULL(
        auto term = create_uart_terminal(config);
        return std::make_shared<DTE>(std::move(term));
    )
}

template<typename SpecificModule>
std::unique_ptr<DCE_T<SpecificModule>> create_dce(const std::shared_ptr<DTE>& dte, const std::shared_ptr<SpecificModule>& dev, esp_netif_t *netif)
{
    TRY_CATCH_RET_NULL(
        return std::make_unique<DCE_T<SpecificModule>>(dte, dev, netif);
    )
}

std::unique_ptr<DCE> create_generic_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<GenericModule>& dev, esp_netif_t *netif)
{
    TRY_CATCH_RET_NULL(
            return std::make_unique<DCE>(dte, dev, netif);
    )
}

//std::unique_ptr<DCE> create_SIM7600_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn)
//{
//    auto pdp = std::make_unique<PdpContext>(apn);
//    auto dev = std::make_shared<SIM7600>(dte, std::move(pdp));
//
//    return create_generic_dce_from_module(dte, std::move(dev), netif);
//}


static inline std::unique_ptr<DCE> create_modem_dce(dce_factory::Modem m, const esp_modem_dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif)
{
    dce_factory::Factory f(m);
    TRY_CATCH_RET_NULL(
        return f.build_unique(config, std::move(dte), netif);
    )
}

std::unique_ptr<DCE> create_SIM7600_dce(const esp_modem_dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif)
{
    return create_modem_dce(dce_factory::Modem::SIM7600, config, std::move(dte), netif);
}

std::unique_ptr<DCE> create_SIM800_dce(const esp_modem_dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif)
{
    return create_modem_dce(dce_factory::Modem::SIM800, config, std::move(dte), netif);
}

std::unique_ptr<DCE> create_BG96_dce(const esp_modem_dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif)
{
    return create_modem_dce(dce_factory::Modem::BG96, config, std::move(dte), netif);
}

//
// C API definitions


struct esp_modem_dce_wrap // need to mimic the polymorphic dispatch as CPP uses templated dispatch
{
    enum esp_modem_t { MODEM_SIM7600, MODEM_SIM800, MODEM_BG96 } modem_type;
    void * dce_ptr;
};

static inline esp_err_t command_response_to_esp_err(command_result res)
{
    switch (res) {
        case command_result::OK:
            return ESP_OK;
        case command_result::FAIL:
            return ESP_FAIL;
        case command_result::TIMEOUT:
            return ESP_ERR_TIMEOUT;
    }
    return ESP_ERR_INVALID_ARG;
}

extern "C" esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    auto dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    if (dce_wrap == nullptr)
        return nullptr;
    auto dte = create_uart_dte(dte_config);
    auto dce = create_SIM7600_dce(dce_config, dte, netif);
    dce_wrap->modem_type = esp_modem_dce_wrap::MODEM_SIM7600;
    dce_wrap->dce_ptr = dce.release();
    return dce_wrap;
}

extern "C" void esp_modem_destroy(esp_modem_dce_t * dce)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    delete dce_sim7600;
    delete dce;
}

extern "C" esp_err_t esp_modem_set_mode(esp_modem_dce_t * dce, esp_modem_dce_mode_t mode)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    if (mode == ESP_MODEM_MODE_DATA) {
        dce_sim7600->set_data();
    } else if (mode == ESP_MODEM_MODE_COMMAND) {
        dce_sim7600->set_data();
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_read_pin(esp_modem_dce_t * dce, bool &x)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE*>(dce->dce_ptr);
    return command_response_to_esp_err(dce_sim7600->read_pin(x));
}

// define all commands manually, as we have to convert char* strings to CPP strings and references to