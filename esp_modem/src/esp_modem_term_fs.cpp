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

#include <sys/fcntl.h>
#include "cxx_include/esp_modem_dte.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "esp_vfs_dev.h"

static const char *TAG = "fs_terminal";

namespace esp_modem::terminal {

struct uart_resource {
    explicit uart_resource(const esp_modem_dte_config *config);
    ~uart_resource();
    uart_port_t port;
    int fd;
};

struct vfs_task {
    explicit vfs_task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function) :
            task_handle(nullptr) {
        BaseType_t ret = xTaskCreate(task_function, "vfs_task"
                                                    "", stack_size, task_param, priority, &task_handle);
        throw_if_false(ret == pdTRUE, "create uart event task failed");
    }

    ~vfs_task() {
        if (task_handle) vTaskDelete(task_handle);
    }

    TaskHandle_t task_handle;       /*!< UART event task handle */
};

uart_resource::~uart_resource()
{
    if (port >= UART_NUM_0 && port < UART_NUM_MAX) {
        uart_driver_delete(port);
    }
    if (fd >= 0) {
        close(fd);
    }
}

uart_resource::uart_resource(const esp_modem_dte_config *config) :
        port(-1), fd(-1)
{
    /* Config UART */
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->vfs_config.baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_REF_TICK;

    throw_if_esp_fail(uart_param_config(config->vfs_config.port_num, &uart_config), "config uart parameter failed");

    throw_if_esp_fail(uart_set_pin(config->vfs_config.port_num, config->vfs_config.tx_io_num, config->vfs_config.rx_io_num,
                                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), "config uart gpio failed");

    throw_if_esp_fail(uart_driver_install(config->vfs_config.port_num, config->vfs_config.rx_buffer_size, config->vfs_config.tx_buffer_size,
                              0, nullptr, 0), "install uart driver failed");

    throw_if_esp_fail(uart_set_rx_timeout(config->vfs_config.port_num, 1), "set rx timeout failed");

    throw_if_esp_fail(uart_set_rx_full_threshold(config->uart_config.port_num, 64), "config rx full threshold failed");

    /* mark UART as initialized */
    port = config->vfs_config.port_num;

    fd = open(config->vfs_config.dev_name, O_RDWR);

    throw_if_false(fd >= 0, "Cannot open the fd");
}

class vfs_terminal : public Terminal {
public:
    explicit vfs_terminal(const esp_modem_dte_config *config) :
            uart(config), signal(),
            task_handle(config->vfs_config.task_stack_size, config->vfs_config.task_prio, this, s_task) {}

    ~vfs_terminal() override = default;

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
        auto t = static_cast<vfs_terminal *>(task_param);
        t->task();
        vTaskDelete(NULL);
    }

    void task();

    static const size_t TASK_INIT = BIT0;
    static const size_t TASK_START = BIT1;
    static const size_t TASK_STOP = BIT2;
    static const size_t TASK_PARAMS = BIT3;

    uart_resource uart;
    signal_group signal;
    vfs_task task_handle;
};

std::unique_ptr<Terminal> create_vfs_terminal(const esp_modem_dte_config *config) {
    TRY_CATCH_RET_NULL(
            auto term = std::make_unique<vfs_terminal>(config);
            term->start();
            return term;
    )
}

void vfs_terminal::task() {
    std::function<bool(uint8_t *data, size_t len)> on_data_priv = nullptr;
    size_t len;
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // exits to the static method where the task gets deleted
    }
    esp_vfs_dev_uart_use_driver(uart.port);

    while (signal.is_any(TASK_START)) {
        int s;
        fd_set rfds;
        struct timeval tv = {
                .tv_sec = 5,
                .tv_usec = 0,
        };
        FD_ZERO(&rfds);
        FD_SET(uart.fd, &rfds);

        s = select(uart.fd + 1, &rfds, NULL, NULL, &tv);
//        if (signal.is_any(TASK_PARAMS)) {
            on_data_priv = on_data;
//            signal.clear(TASK_PARAMS);
//        }

        if (s < 0) {
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            break;
        } else if (s == 0) {
            ESP_LOGI(TAG, "Timeout has been reached and nothing has been received");
        } else {
            if (FD_ISSET(uart.fd, &rfds)) {
                uart_get_buffered_data_len(uart.port, &len);
                if (len && on_data_priv) {
                    if (on_data_priv(nullptr, len)) {
                        on_data_priv = nullptr;
                    }
                }
            }
        }
        vTaskDelay(1);
    }
}

int vfs_terminal::read(uint8_t *data, size_t len) {
    return ::read(uart.fd, data, len);
}

int vfs_terminal::write(uint8_t *data, size_t len) {
    return ::write(uart.fd, data, len);
}

} // namespace esp_modem

