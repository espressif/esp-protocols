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

#include "cxx_include/esp_modem_dte.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"

#define ESP_MODEM_EVENT_QUEUE_SIZE (16)

static const char *TAG = "uart_terminal";

namespace esp_modem {

struct uart_resource {
    explicit uart_resource(const esp_modem_dte_config *config);

    ~uart_resource();

    bool get_event(uart_event_t &event, uint32_t time_ms) {
        return xQueueReceive(event_queue, &event, pdMS_TO_TICKS(time_ms));
    }

    void reset_events() {
        uart_flush_input(port);
        xQueueReset(event_queue);
    }

    uart_port_t port;                  /*!< UART port */
    QueueHandle_t event_queue;              /*!< UART event queue handle */
    int line_buffer_size;                   /*!< line buffer size in command mode */
    int pattern_queue_size;                 /*!< UART pattern queue size */
};

struct uart_task {
    explicit uart_task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function) :
            task_handle(nullptr) {
        BaseType_t ret = xTaskCreate(task_function, "uart_task", 10000, task_param, priority, &task_handle);
        throw_if_false(ret == pdTRUE, "create uart event task failed");
    }

    ~uart_task() {
        if (task_handle) vTaskDelete(task_handle);
    }

    TaskHandle_t task_handle;       /*!< UART event task handle */
};


struct uart_event_loop {
    explicit uart_event_loop() : event_loop_hdl(nullptr) {
        esp_event_loop_args_t loop_args = {};
        loop_args.queue_size = ESP_MODEM_EVENT_QUEUE_SIZE;
        loop_args.task_name = nullptr;
        throw_if_esp_fail(esp_event_loop_create(&loop_args, &event_loop_hdl), "create event loop failed");
    }

    void run() { esp_event_loop_run(event_loop_hdl, pdMS_TO_TICKS(0)); }

    ~uart_event_loop() { if (event_loop_hdl) esp_event_loop_delete(event_loop_hdl); }

    esp_event_loop_handle_t event_loop_hdl;
};

uart_resource::~uart_resource() {
    if (port >= UART_NUM_0 && port < UART_NUM_MAX) {
        uart_driver_delete(port);
    }
}


uart_resource::uart_resource(const esp_modem_dte_config *config) :
        port(-1) {
    esp_err_t res;
    line_buffer_size = config->line_buffer_size;

    /* Config UART */
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->baud_rate;
    uart_config.data_bits = config->data_bits;
    uart_config.parity = config->parity;
    uart_config.stop_bits = config->stop_bits;
    uart_config.flow_ctrl = (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) ? UART_HW_FLOWCTRL_CTS_RTS
                                                                                : UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_REF_TICK;

    throw_if_esp_fail(uart_param_config(config->port_num, &uart_config), "config uart parameter failed");

    if (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        res = uart_set_pin(config->port_num, config->tx_io_num, config->rx_io_num,
                           config->rts_io_num, config->cts_io_num);
    } else {
        res = uart_set_pin(config->port_num, config->tx_io_num, config->rx_io_num,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    throw_if_esp_fail(res, "config uart gpio failed");
    /* Set flow control threshold */
    if (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        res = uart_set_hw_flow_ctrl(config->port_num, UART_HW_FLOWCTRL_CTS_RTS, UART_FIFO_LEN - 8);
    } else if (config->flow_control == ESP_MODEM_FLOW_CONTROL_SW) {
        res = uart_set_sw_flow_ctrl(config->port_num, true, 8, UART_FIFO_LEN - 8);
    }
    throw_if_esp_fail(res, "config uart flow control failed");
    /* Install UART driver and get event queue used inside driver */
    res = uart_driver_install(config->port_num, config->rx_buffer_size, config->tx_buffer_size,
                              config->event_queue_size, &(event_queue), 0);
    throw_if_esp_fail(res, "install uart driver failed");
    throw_if_esp_fail(uart_set_rx_timeout(config->port_num, 1), "set rx timeout failed");

    uart_set_rx_full_threshold(config->port_num, 64);
    throw_if_esp_fail(res, "config uart pattern failed");
    /* mark UART as initialized */
    port = config->port_num;
}

class uart_terminal : public Terminal {
public:
    explicit uart_terminal(const esp_modem_dte_config *config) :
            uart(config), event_loop(), signal(),
            task_handle(config->event_task_stack_size, config->event_task_priority, this, s_task) {}

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
        vTaskDelete(NULL);
    }

    void task();

    static const size_t TASK_INIT = BIT0;
    static const size_t TASK_START = BIT1;
    static const size_t TASK_STOP = BIT2;
    static const size_t TASK_PARAMS = BIT3;


    uart_resource uart;
    uart_event_loop event_loop;
    signal_group signal;
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
        event_loop.run();
        if (uart.get_event(event, 100)) {
            if (signal.is_any(TASK_PARAMS)) {
                on_data_priv = on_data;
                signal.clear(TASK_PARAMS);
            }
            switch (event.type) {
                case UART_DATA:
//                    ESP_LOGI(TAG, "UART_DATA");
//                    ESP_LOG_BUFFER_HEXDUMP("esp-modem-pattern: debug_data", esp_dte->buffer, length, ESP_LOG_DEBUG);
                    uart_get_buffered_data_len(uart.port, &len);
//                    ESP_LOGI(TAG, "UART_DATA len=%d, on_data=%d", len, (bool)on_data);
                    if (len && on_data_priv) {
                        if (on_data_priv(nullptr, len)) {
                            on_data_priv = nullptr;
                        }
                    }
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "HW FIFO Overflow");
                    uart.reset_events();
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring Buffer Full");
                    uart.reset_events();
                    break;
                case UART_BREAK:
                    ESP_LOGW(TAG, "Rx Break");
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "Parity Error");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "Frame Error");
                    break;
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "UART_PATTERN_DET");
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
    if (length > 0) {
        return uart_read_bytes(uart.port, data, length, portMAX_DELAY);
    }
    return 0;
}

int uart_terminal::write(uint8_t *data, size_t len) {
    return uart_write_bytes(uart.port, data, len);
}

} // namespace esp_modem

