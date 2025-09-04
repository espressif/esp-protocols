/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file urc_test.cpp
 * @brief Enhanced URC (Unsolicited Result Code) Test
 *
 * This test demonstrates the new enhanced URC interface with buffer consumption control.
 * It tests the following features:
 *
 * 1. Enhanced URC Handler Registration: Uses set_enhanced_urc() instead of set_urc()
 * 2. Buffer Visibility: URC handler receives complete buffer information
 * 3. Granular Consumption Control: Handler can consume none, partial, or all buffer data
 * 4. Processing State Awareness: Handler knows what data is new vs. already processed
 * 5. Command State Awareness: Handler knows if a command is currently active
 *
 * The test works with ESP-AT HTTP server that sends chunked responses, demonstrating
 * how the enhanced URC handler can process multi-part responses with precise control
 * over buffer consumption.
 */
#include <cstring>
#include <algorithm>
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
        set_enhanced_urc(handle_enhanced_urc);
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
    static esp_modem::DTE::UrcConsumeInfo handle_enhanced_urc(const esp_modem::DTE::UrcBufferInfo &info)
    {
        // Log buffer information for debugging
        ESP_LOGD(TAG, "URC Buffer Info: total_size=%zu, processed_offset=%zu, new_data_size=%zu, command_active=%s",
                 info.buffer_total_size, info.processed_offset, info.new_data_size,
                 info.is_command_active ? "true" : "false");

        // Debug: Show buffer content (first 200 chars)
        if (info.buffer_total_size > 0) {
            size_t debug_len = std::min(info.buffer_total_size, (size_t)200);
            ESP_LOGD(TAG, "Buffer content (first %zu chars): %.*s",
                     debug_len, (int)debug_len, (const char*)info.buffer_start);
        }

        // Create string view of the entire buffer for processing
        std::string_view buffer((const char*)info.buffer_start, info.buffer_total_size);

        // First, check if we have the completion message anywhere in the buffer
        if (buffer.find("Transfer completed") != std::string_view::npos) {
            ESP_LOGI(TAG, "Transfer completed detected in buffer!");
            xEventGroupSetBits(s_event_group, transfer_completed);
            // Consume everything
            return {esp_modem::DTE::UrcConsumeResult::CONSUME_ALL, 0};
        }

        // Process from the last processed offset
        size_t search_start = info.processed_offset;

        // Look for complete lines starting from the processed offset
        while (search_start < info.buffer_total_size) {
            size_t line_end = buffer.find('\n', search_start);

            if (line_end == std::string_view::npos) {
                // No complete line found, wait for more data
                ESP_LOGD(TAG, "Waiting for more data... (search_start=%zu, total_size=%zu)",
                         search_start, info.buffer_total_size);
                return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
            }

            // Found a complete line, process it
            std::string_view line = buffer.substr(search_start, line_end - search_start);

            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }

            // Check if this is an HTTP URC
            if (line.starts_with("+HTTPCGET:")) {
                ESP_LOGI(TAG, "HTTP URC: %.*s", (int)line.length(), line.data());

                // Check for transfer completion - look for "Transfer completed" anywhere in the line
                if (line.find("Transfer completed") != std::string_view::npos) {
                    ESP_LOGI(TAG, "Transfer completed detected!");
                    xEventGroupSetBits(s_event_group, transfer_completed);
                }

                // Consume this line (including the newline)
                size_t consume_size = line_end + 1 - info.processed_offset;
                ESP_LOGD(TAG, "Consuming %zu bytes (line_end=%zu, processed_offset=%zu)",
                         consume_size, line_end, info.processed_offset);
                return {esp_modem::DTE::UrcConsumeResult::CONSUME_PARTIAL, consume_size};

            } else if (line.starts_with("+HTTPCGET")) {
                // Partial HTTP URC, don't consume yet
                ESP_LOGD(TAG, "Partial HTTP URC: %.*s", (int)line.length(), line.data());
                return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};

            } else if (!line.empty()) {
                // Other data, log and consume
                ESP_LOGD(TAG, "Other data: %.*s", (int)line.length(), line.data());
                size_t consume_size = line_end + 1 - info.processed_offset;
                return {esp_modem::DTE::UrcConsumeResult::CONSUME_PARTIAL, consume_size};
            }

            // Move to next line
            search_start = line_end + 1;
        }

        // Processed all available data
        ESP_LOGD(TAG, "Processed all available data");
        return {esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
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
    dte_config.uart_config.tx_io_num = 17;
    dte_config.uart_config.rx_io_num = 18;
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

    ESP_LOGI(TAG, "Starting Enhanced URC Test");
    ESP_LOGI(TAG, "This test demonstrates the new enhanced URC interface with buffer consumption control");

    dce->start_http_server();

    ESP_LOGI(TAG, "Sending HTTP GET request with enhanced URC handler");
    dce->http_get("http://127.0.0.1:8080/async");

    EventBits_t bits = xEventGroupWaitBits(s_event_group, 1, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & DCE::transfer_completed) {
        ESP_LOGI(TAG, "Enhanced URC test completed successfully!");
        ESP_LOGI(TAG, "The enhanced URC handler successfully processed all HTTP chunks");
        ESP_LOGI(TAG, "with granular buffer consumption control");
    } else {
        ESP_LOGE(TAG, "Enhanced URC test timed out");
    }

    dce->sync();
    vEventGroupDelete(s_event_group);
    ESP_LOGI(TAG, "Enhanced URC test done");
}
