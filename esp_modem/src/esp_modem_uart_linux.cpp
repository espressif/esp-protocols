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
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"

namespace esp_modem::terminal {

constexpr const char *TAG = "uart_term";

struct uart_resource {
    explicit uart_resource(const esp_modem_dte_config *config);

    ~uart_resource();

    uart_port_t port{};
    int fd;
};

uart_resource::uart_resource(const esp_modem_dte_config *config)
{
    ESP_LOGD(TAG, "Creating uart resource" );
    struct termios tty = {};
    fd = open( config->vfs_config.dev_name, O_RDWR | O_NOCTTY | O_NONBLOCK );
    throw_if_false(fd >= 0 , "Failed to open serial port");
    throw_if_false(tcgetattr(fd, &tty) == 0, "Failed to tcgetattr()");

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE; // Clear all the size bits, then use one of the statements below
    tty.c_cflag |= CS8; // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    cfsetspeed(&tty, B115200);
    ioctl(fd, TCSETS, &tty);
}

uart_resource::~uart_resource()
{
 close(fd);
}

}