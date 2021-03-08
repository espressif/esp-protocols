//
// Created by david on 3/3/21.
//
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_terminal.hpp"
#include "esp_log.h"
#include "cxx_include/esp_modem_api.hpp"
#include "esp_modem_api.h"
#include "esp_modem_config.h"

static const char *TAG = "dce_factory";
struct PdpContext;

std::shared_ptr<DTE> create_uart_dte(const esp_modem_dte_config *config)
{
    try {
        auto term = create_uart_terminal(config);
        return std::make_shared<DTE>(std::move(term));
    } catch (std::bad_alloc& e) {
        ESP_LOGE(TAG, "Out of memory");
        return nullptr;
    } catch (esp_err_exception& e) {
        esp_err_t err = e.get_err_t();
        ESP_LOGE(TAG, "Error occurred during UART term init: %d", err);
        ESP_LOGE(TAG, "%s", e.what());
        return nullptr;
    }

}

template<typename SpecificModule>
std::unique_ptr<DCE<SpecificModule>> create_dce(const std::shared_ptr<DTE>& dte, const std::shared_ptr<SpecificModule>& dev, esp_netif_t *netif)
{
    try {
        return std::make_unique<DCE<SpecificModule>>(dte, dev, netif);
    } catch (std::bad_alloc& e) {
        ESP_LOGE(TAG, "Out of memory");
        return nullptr;
    } catch (esp_err_exception& e) {
        esp_err_t err = e.get_err_t();
        ESP_LOGE(TAG, "Error occurred during UART term init: %d", err);
        ESP_LOGE(TAG, "%s", e.what());
        return nullptr;
    }
}

std::unique_ptr<DCE<GenericModule>> create_generic_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<GenericModule>& dev, esp_netif_t *netif)
{
    return create_dce<GenericModule>(dte, dev, netif);
}

std::unique_ptr<DCE<SIM7600>> create_SIM7600_dce_from_module(const std::shared_ptr<DTE>& dte, const std::shared_ptr<SIM7600>& dev, esp_netif_t *netif)
{
    return create_dce<SIM7600>(dte, dev, netif);
}

std::unique_ptr<DCE<SIM7600>> create_SIM7600_dce(const std::shared_ptr<DTE>& dte, esp_netif_t *netif, std::string &apn)
{
    auto pdp = std::make_unique<PdpContext>(apn);
    auto dev = std::make_shared<SIM7600>(dte, std::move(pdp));

    return create_dce<SIM7600>(dte, std::move(dev), netif);
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

extern "C" esp_modem_t *esp_modem_new(const esp_modem_dte_config *config, esp_netif_t *netif, const char* apn)
{
    auto dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    if (dce_wrap == nullptr)
        return nullptr;
    std::string apn_str(apn);
    auto dte = create_uart_dte(config);
    auto dce = create_SIM7600_dce(dte, netif, apn_str);
    dce_wrap->modem_type = esp_modem_dce_wrap::MODEM_SIM7600;
    dce_wrap->dce_ptr = dce.release();
    return dce_wrap;
}

extern "C" void esp_modem_destroy(esp_modem_t * dce)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE<SIM7600>*>(dce->dce_ptr);
    delete dce_sim7600;
    delete dce;
}

extern "C" esp_err_t esp_modem_read_pin(esp_modem_t * dce, bool &x)
{
    assert(dce->modem_type == esp_modem_dce_wrap::MODEM_SIM7600);
    auto dce_sim7600 = static_cast<DCE<SIM7600>*>(dce->dce_ptr);
    return command_response_to_esp_err(dce_sim7600->read_pin(x));
}

// define all commands manually, as we have to convert char* strings to CPP strings and references to