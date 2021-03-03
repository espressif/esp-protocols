//
// Created by david on 3/3/21.
//
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/uart_terminal.hpp"
#include "esp_log.h"
#include "cxx_include/esp_modem_api.hpp"

static const char *TAG = "dce_factory";
struct PdpContext;

std::shared_ptr<DTE> create_uart_dte(const dte_config *config)
{
    try {
//        auto term = std::make_unique<dte>(std::make_unique<uart_terminal>(config));
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

std::unique_ptr<DCE> create_dce(const std::shared_ptr<DTE>& dte, const std::shared_ptr<DeviceIf>& dev, esp_netif_t *netif)
{
    try {
        return std::make_unique<DCE>(dte, dev, netif);
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

