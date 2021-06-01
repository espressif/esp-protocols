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

#include <optional>
#include <unistd.h>
#include <sys/fcntl.h>
#include "esp_log.h"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_exception.hpp"
#include "exception_stub.hpp"
#include "uart_resource.hpp"
#include "vfs_resource/vfs_create.hpp"

constexpr const char *TAG = "vfs_uart_creator";


struct esp_modem_vfs_resource {
    explicit esp_modem_vfs_resource(const esp_modem_uart_term_config *config, int fd)
        : internal(config, nullptr, fd) {}

    esp_modem::uart_resource internal;
};


static void vfs_destroy_uart(int fd, struct esp_modem_vfs_resource *resource)
{
    if (fd >= 0) {
        close(fd);
    }
    delete resource;
}

bool vfs_create_uart(struct esp_modem_vfs_uart_creator *config, struct esp_modem_vfs_term_config *created_config)
{
    if (!config->dev_name || created_config == nullptr) {
        return false;
    }
    TRY_CATCH_OR_DO(
        int fd = open(config->dev_name, O_RDWR);
        esp_modem::throw_if_false(fd >= 0, "Cannot open the fd");

        created_config->resource = new esp_modem_vfs_resource(&config->uart, fd);
        created_config->fd = fd;
        created_config->deleter = vfs_destroy_uart;

        // Set the FD to non-blocking mode
        int flags = fcntl(fd, F_GETFL, nullptr) | O_NONBLOCK;
        fcntl(fd, F_SETFL, flags);

        , return false)

    return true;
}
