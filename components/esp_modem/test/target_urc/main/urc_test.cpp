/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_command_library_utils.hpp"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "urc_test";
static EventGroupHandle_t s_event_group = nullptr;

class ESP_AT_Module: public ::esp_modem::ModuleIf {
public:
    explicit ESP_AT_Module(std::shared_ptr<::esp_modem::DTE> dte, const esp_modem_dce_config *config):
        dte(std::move(dte)) {}

    bool setup_data_mode() override
    {
        // not using network here
        return true;
    }

    bool set_mode(::esp_modem::modem_mode mode) override
    {
        // we never allow mode change
        return false;
    }

protected:
    std::shared_ptr<::esp_modem::DTE> dte;
};

class DCE : public esp_modem::DCE_T<ESP_AT_Module> {
    using DCE_T<ESP_AT_Module>::DCE_T;
public:


    bool init()
    {
        for (int i = 0; i < 5; ++i) {
            if (sync() == esp_modem::command_result::OK) {
                ESP_LOGI(TAG, "Modem in sync");
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(500 * (i + 1)));
        }
        ESP_LOGE(TAG, "Failed to sync with esp-at");
        return false;
    }

    esp_modem::command_result sync()
    {
        auto ret = esp_modem::dce_commands::generic_command_common(dte.get(), "AT\r\n");
        ESP_LOGI(TAG, "Syncing with esp-at...(%d)", static_cast<int>(ret));
        return ret;
    }

    bool http_get(const std::string &url)
    {
        std::string command = "AT+HTTPCGET=\"" + url + "\"\r\n";
        set_urc(handle_urc);
        auto ret = dte->write(esp_modem::DTE_Command(command));
        ESP_LOGI(TAG, "HTTP GET...(%d)", static_cast<int>(ret));
        return ret > 0;
    }

    bool start_http_server() const
    {
        auto ret = esp_modem::dce_commands::generic_command_common(dte.get(), "AT+HTTPD\r\n");
        ESP_LOGI(TAG, "Start HTTP server...(%d)", static_cast<int>(ret));
        return ret == esp_modem::command_result::OK;
    }

    static constexpr int transfer_completed = 1;
private:
    static esp_modem::command_result handle_urc(uint8_t *data, size_t len)
    {
        static int start_chunk = 0;
        static int end_chunk = 0;
        std::string_view chunk((const char*)data + start_chunk, len - start_chunk);
        int newline = chunk.find('\n');
        if (newline == std::string_view::npos) {
            end_chunk = len;    // careful, this grows buffer usage
            printf(".");
            return esp_modem::command_result::TIMEOUT;
        }
        printf("%.*s\n", newline, (char*)data + start_chunk);
        start_chunk = end_chunk;
        // check for the last one
        constexpr char last_chunk[] = "Transfer completed";
        if (memmem(data, len, last_chunk, sizeof(last_chunk) - 1) != nullptr) {
            xEventGroupSetBits(s_event_group, transfer_completed);
        }
        return esp_modem::command_result::OK;
    }
};

class Factory: public ::esp_modem::dce_factory::Factory {
public:
    static std::unique_ptr<DCE> create(const esp_modem::dce_config *config, std::shared_ptr<esp_modem::DTE> dte, esp_netif_t *netif)
    {
        return build_generic_DCE<ESP_AT_Module, DCE, std::unique_ptr<DCE>>(config, std::move(dte), netif);
    }
};

std::unique_ptr<DCE> create(std::shared_ptr<esp_modem::DTE> dte)
{
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    static esp_netif_t *netif = esp_netif_new(&netif_ppp_config);
    assert(netif);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("APN"); // dummy config (not used with esp-at)
    return Factory::create(&dce_config, std::move(dte), netif);
}

extern "C" void app_main(void)
{
    /* Init and register system/core components */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_event_group = xEventGroupCreate();

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.dte_buffer_size = 1024;
    dte_config.uart_config.tx_io_num = 18;
    dte_config.uart_config.rx_io_num = 17;
    auto uart_dte = esp_modem::create_uart_dte(&dte_config);
    if (uart_dte == nullptr) {
        ESP_LOGE(TAG, "Failed to create UART DTE");
        return;
    }
    auto dce = create(std::move(uart_dte));
    if (!dce->init()) {
        ESP_LOGE(TAG,  "Failed to setup network");
        return;
    }

    dce->start_http_server();

    dce->http_get("http://127.0.0.1:8080/async");

    EventBits_t bits = xEventGroupWaitBits(s_event_group, 1, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & DCE::transfer_completed) {
        ESP_LOGI(TAG, "Request finished!");
    }
    dce->sync();
    vEventGroupDelete(s_event_group);
    ESP_LOGI(TAG, "Done");
}
