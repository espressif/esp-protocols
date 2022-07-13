// Copyright 2021 Espressif Systems (Shanghai) CO LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_modem_config.h"
#include "esp_modem_usb_config.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "sdkconfig.h"
#include "usb_terminal.hpp"

static const char *TAG = "usb_terminal";

/**
 * @brief USB Host task
 * 
 * This task is created only if install_usb_host is set to true in DTE configuration.
 * In case the user doesn't want in install USB Host driver here, he must install it before creating UsbTerminal object.
 * 
 * @param arg Unused
 */
void usb_host_task(void *arg)
{
    while (1) {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGD(TAG, "No more clients: clean up\n");
            usb_host_device_free_all();
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGD(TAG, "All free: uninstall USB lib\n");
            break;
        }
    }

    // Clean up USB Host
    vTaskDelay(10); // Short delay to allow clients clean-up
    usb_host_lib_handle_events(0, NULL); // Make sure there are now pending events
    usb_host_uninstall();
    vTaskDelete(NULL);
}

namespace esp_modem {
class UsbTerminal : public Terminal, private CdcAcmDevice {
public:
    explicit UsbTerminal(const esp_modem_dte_config *config)
    {
        const struct esp_modem_usb_term_config* usb_config = (struct esp_modem_usb_term_config*)(config->extension_config);

        // Install USB Host driver
        if (usb_config->install_usb_host) {
            const usb_host_config_t host_config = {
                .skip_phy_setup = false,
                .intr_flags = ESP_INTR_FLAG_LEVEL1,
            };
            ESP_MODEM_THROW_IF_ERROR(usb_host_install(&host_config), "USB Host install failed");
            ESP_LOGD(TAG, "USB Host installed");
            ESP_MODEM_THROW_IF_FALSE(pdTRUE == xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, NULL, config->task_priority + 1, NULL, usb_config->xCoreID), "USB host task failed");
        }

        // Install CDC-ACM driver
        const cdc_acm_host_driver_config_t esp_modem_cdc_acm_driver_config = {
            .driver_task_stack_size = config->task_stack_size,
            .driver_task_priority = config->task_priority,
            .xCoreID = (BaseType_t)usb_config->xCoreID
        };

        // Silently continue of error: CDC-ACM driver might be already installed
        cdc_acm_host_install(&esp_modem_cdc_acm_driver_config);
        
        // Open CDC-ACM device
        const cdc_acm_host_device_config_t esp_modem_cdc_acm_device_config = {
            .connection_timeout_ms = usb_config->timeout_ms,       
            .out_buffer_size = config->dte_buffer_size,               
            .event_cb = handle_notif,
            .data_cb = handle_rx,      
            .user_arg = this                       
        };

        if (usb_config->cdc_compliant) {
            ESP_MODEM_THROW_IF_ERROR(this->CdcAcmDevice::open(usb_config->vid, usb_config->pid,
                            usb_config->interface_idx, &esp_modem_cdc_acm_device_config),
                            "USB Device open failed");
        } else {
            ESP_MODEM_THROW_IF_ERROR(this->CdcAcmDevice::open_vendor_specific(usb_config->vid, usb_config->pid,
                            usb_config->interface_idx, &esp_modem_cdc_acm_device_config),
                            "USB Device open failed");
        }
    };

    ~UsbTerminal()
    {
        this->CdcAcmDevice::close();
    };

    void start() override
    {
        return;
    }

    void stop() override
    {
        return;
    }

    int write(uint8_t *data, size_t len) override
    {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG);
        if (this->CdcAcmDevice::tx_blocking(data, len) != ESP_OK) {
            return -1;
        }
        return len;
    }

    int read(uint8_t *data, size_t len) override
    {
        // This function should never be called. UsbTerminal provides data through Terminal::on_read callback
        ESP_LOGW(TAG, "Unexpected call to UsbTerminal::read function");
        return -1;
    }

private:
    UsbTerminal() = delete;
    UsbTerminal(const UsbTerminal &copy) = delete;
    UsbTerminal &operator=(const UsbTerminal &copy) = delete;
    bool operator== (const UsbTerminal &param) const = delete;
    bool operator!= (const UsbTerminal &param) const = delete;

    static void handle_rx(uint8_t *data, size_t data_len, void *user_arg)
    {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_DEBUG);
        UsbTerminal *this_terminal = static_cast<UsbTerminal *>(user_arg);
        if (data_len > 0 && this_terminal->on_read) {
            this_terminal->on_read(data, data_len);
        } else {
            ESP_LOGD(TAG, "Unhandled RX data");
        }
    }

    static void handle_notif(cdc_acm_dev_hdl_t cdc_hdl, const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
    {
        UsbTerminal *this_terminal = static_cast<UsbTerminal *>(user_ctx);
        
        switch (event->type) {
        // Notifications like Ring, Rx Carrier indication or Network connection indication are not relevant for USB terminal
        case CDC_ACM_HOST_NETWORK_CONNECTION:
        case CDC_ACM_HOST_SERIAL_STATE:
            break;
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:
            ESP_LOGW(TAG, "USB terminal disconnected");
            cdc_acm_host_close(cdc_hdl);
            if (this_terminal->on_error) {
                this_terminal->on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
            }
            break;
        case CDC_ACM_HOST_ERROR:
            ESP_LOGE(TAG, "Unexpected CDC-ACM error: %d.", event->data.error);
            if (this_terminal->on_error) {
                this_terminal->on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
            }
            break;
        default:
            abort();
        }
    };
};

std::unique_ptr<Terminal> create_usb_terminal(const esp_modem_dte_config *config)
{
    TRY_CATCH_RET_NULL(
        return std::make_unique<UsbTerminal>(config);
    )
}
} // namespace esp_modem
