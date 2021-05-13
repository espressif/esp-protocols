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

#include <unistd.h>
#include <sys/fcntl.h>
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"

static const char *TAG = "fs_terminal";

namespace esp_modem::terminal {

struct uart_resource {
    explicit uart_resource(const esp_modem_dte_config *config);
    ~uart_resource();
    uart_port_t port;
    int fd;
};


class vfs_terminal : public Terminal {
public:
    explicit vfs_terminal(const esp_modem_dte_config *config) :
            uart(config), signal(),
            task_handle(config->vfs_config.task_stack_size, config->vfs_config.task_prio, this, [](void* p){
                auto t = static_cast<vfs_terminal *>(p);
                t->task();
                Task::Delete();
            }) {}

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
    void task();

    static const size_t TASK_INIT = SignalGroup::bit0;
    static const size_t TASK_START = SignalGroup::bit1;
    static const size_t TASK_STOP = SignalGroup::bit2;
    static const size_t TASK_PARAMS = SignalGroup::bit3;

    uart_resource uart;
    SignalGroup signal;
    Task task_handle;
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
//    size_t len;
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // exits to the static method where the task gets deleted
    }
//    esp_vfs_dev_uart_use_driver(uart.port);
    int flags = fcntl(uart.fd, F_GETFL, NULL) | O_NONBLOCK;
    fcntl(uart.fd, F_SETFL, flags);

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
        if (signal.is_any(TASK_PARAMS)) {
            on_data_priv = on_data;
            signal.clear(TASK_PARAMS);
        }

        if (s < 0) {
            break;
        } else if (s == 0) {

        } else {
            if (FD_ISSET(uart.fd, &rfds)) {
//                uart_get_buffered_data_len(uart.port, &len);
                if (on_data_priv) {
                    if (on_data_priv(nullptr, 0)) {
                        on_data_priv = nullptr;
                    }
                }
            }
        }
        Task::Relinquish();
    }
}

int vfs_terminal::read(uint8_t *data, size_t len)
{
    return ::read(uart.fd, data, len);
}

int vfs_terminal::write(uint8_t *data, size_t len)
{
    return ::write(uart.fd, data, len);
}

} // namespace esp_modem

