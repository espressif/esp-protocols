// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
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
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_resource.hpp"

static const char *TAG = "uart_terminal";

namespace esp_modem {


struct uart_task {
    explicit uart_task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function) :
            task_handle(nullptr) {
        BaseType_t ret = xTaskCreate(task_function, "uart_task", stack_size, task_param, priority, &task_handle);
        throw_if_false(ret == pdTRUE, "create uart event task failed");
    }

    ~uart_task() {
        if (task_handle) vTaskDelete(task_handle);
    }

    TaskHandle_t task_handle;       /*!< UART event task handle */
};



class uart_terminal : public Terminal {
public:
    explicit uart_terminal(const esp_modem_dte_config *config) :
            event_queue(), uart(config, &event_queue, -1), signal(),
            task_handle(config->task_stack_size, config->task_priority, this, s_task) {}

    ~uart_terminal() override = default;

    void start() override {
        signal.set(TASK_START);
    }

    void stop() override {
        signal.set(TASK_STOP);
    }

    int write(uint8_t *data, size_t len) override;

    int read(uint8_t *data, size_t len) override;

    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f) override {
        on_data = std::move(f);
        signal.set(TASK_PARAMS);
    }

private:
    static void s_task(void *task_param) {
        auto t = static_cast<uart_terminal *>(task_param);
        t->task();
        vTaskDelete(nullptr);
    }

    void task();
    bool get_event(uart_event_t &event, uint32_t time_ms) {
        return xQueueReceive(event_queue, &event, pdMS_TO_TICKS(time_ms));
    }

    void reset_events() {
        uart_flush_input(uart.port);
        xQueueReset(event_queue);
    }

    static const size_t TASK_INIT = BIT0;
    static const size_t TASK_START = BIT1;
    static const size_t TASK_STOP = BIT2;
    static const size_t TASK_PARAMS = BIT3;

    QueueHandle_t event_queue;
    uart_resource uart;
    SignalGroup signal;
    uart_task task_handle;
};

std::unique_ptr<Terminal> create_uart_terminal(const esp_modem_dte_config *config) {
    TRY_CATCH_RET_NULL(
            auto term = std::make_unique<uart_terminal>(config);
            term->start();
            return term;
    )
}

void uart_terminal::task() {
    std::function<bool(uint8_t *data, size_t len)> on_data_priv = nullptr;
    uart_event_t event;
    size_t len;
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // exits to the static method where the task gets deleted
    }
    while (signal.is_any(TASK_START)) {
        if (get_event(event, 100)) {
            if (signal.is_any(TASK_PARAMS)) {
                on_data_priv = on_data;
                signal.clear(TASK_PARAMS);
            }
            switch (event.type) {
                case UART_DATA:
                    uart_get_buffered_data_len(uart.port, &len);
                    if (len && on_data_priv) {
                        if (on_data_priv(nullptr, len)) {
                            on_data_priv = nullptr;
                        }
                    }
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "HW FIFO Overflow");
                    if (on_error)
                        on_error(terminal_error::BUFFER_OVERFLOW);
                    reset_events();
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring Buffer Full");
                    if (on_error)
                        on_error(terminal_error::BUFFER_OVERFLOW);
                    reset_events();
                    break;
                case UART_BREAK:
                    ESP_LOGW(TAG, "Rx Break");
                    if (on_error)
                        on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "Parity Error");
                    if (on_error)
                        on_error(terminal_error::CHECKSUM_ERROR);
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "Frame Error");
                    if (on_error)
                        on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
                    break;
                default:
                    ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                    break;
            }
        }
    }
}

int uart_terminal::read(uint8_t *data, size_t len) {
    size_t length = 0;
    uart_get_buffered_data_len(uart.port, &length);
//    if (esp_random() < UINT32_MAX/4 && length > 32) {
//        printf("ahoj!\n");
//        length -= length/4;
//    }
//    size_t new_size = length/2;
    length = std::min(len, length);
    if (length > 0) {
        return uart_read_bytes(uart.port, data, length, portMAX_DELAY);
    }
    return 0;
}

int uart_terminal::write(uint8_t *data, size_t len) {
    return uart_write_bytes(uart.port, data, len);
}

} // namespace esp_modem

