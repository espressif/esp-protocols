/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

static volatile bool running = true;
static int batch_size = 0;      // 0 = send whole response at once
static int batch_delay_ms = 1;  // delay between batches (ms)

static void signal_handler(int sig)
{
    running = false;
}

static std::string process_at_command(const std::string &command)
{
    if (command == "AT\r" || command == "AT") {
        return "OK\r\n";
    }
    if (command == "ATE0\r" || command == "ATE1\r") {
        return "OK\r\n";
    }
    if (command.find("AT+CSQ\r") != std::string::npos) {
        return "+CSQ: 27,99\r\n\r\nOK\r\n";
    }
    if (command.find("AT+CGMM\r") != std::string::npos) {
        return "SimModem-7600\r\n\r\nOK\r\n";
    }
    if (command.find("AT+CGSN\r") != std::string::npos) {
        return "123456789012345\r\n\r\nOK\r\n";
    }
    if (command.find("AT+CIMI\r") != std::string::npos) {
        return "987654321098765\r\n\r\nOK\r\n";
    }
    if (command.find("AT+COPS?\r") != std::string::npos) {
        return "+COPS: 0,0,\"TestOperator\",7\r\n\r\nOK\r\n";
    }
    if (command.find("AT+COPS=3,0\r") != std::string::npos) {
        return "OK\r\n";
    }
    if (command.find("AT+CBC\r") != std::string::npos) {
        return "+CBC: 3800mV\r\n\r\nOK\r\n";
    }
    if (command.find("AT+CPIN=") != std::string::npos) {
        return "OK\r\n";
    }
    if (command.find("AT+CPIN?\r") != std::string::npos) {
        return "+CPIN: READY\r\n\r\nOK\r\n";
    }
    if (command.find("AT+CFUN") != std::string::npos) {
        return "OK\r\n";
    }
    if (command.find("AT") != std::string::npos) {
        return "OK\r\n";
    }
    return "ERROR\r\n";
}

static void print_escaped(const char *prefix, const std::string &s)
{
    printf("%s[%zu]: ", prefix, s.size());
    for (auto c : s) {
        printf(c >= 0x20 ? "%c" : "\\x%02x", (unsigned char)c);
    }
    printf("\n");
}

static void send_response(int fd, const std::string &cmd, const std::string &response)
{
    print_escaped("modem_sim: rx ", cmd);
    if (batch_size > 0) {
        printf("modem_sim: tx [%zu] in batches of %d, delay %dms\n",
               response.size(), batch_size, batch_delay_ms);
    } else {
        print_escaped("modem_sim: tx ", response);
    }
    fflush(stdout);

    size_t total = 0;
    while (total < response.size()) {
        size_t chunk = response.size() - total;
        if (batch_size > 0 && chunk > (size_t)batch_size) {
            chunk = batch_size;
        }
        ssize_t sent = write(fd, response.c_str() + total, chunk);
        if (sent < 0) {
            perror("modem_sim: write error");
            return;
        }
        total += sent;
        if (batch_size > 0 && total < response.size() && batch_delay_ms > 0) {
            usleep(batch_delay_ms * 1000);
        }
    }
}

static void handle_client(int client_fd)
{
    char buf[1024];
    std::string pending;

    printf("modem_sim: client connected\n");
    fflush(stdout);

    struct pollfd pfd = { .fd = client_fd, .events = POLLIN, .revents = 0 };

    while (running) {
        int ret = poll(&pfd, 1, 500);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("modem_sim: poll");
            break;
        }
        if (ret == 0) {
            // Check for "+++" pattern (no \r terminator)
            if (pending.find("+++") != std::string::npos) {
                std::string response = "NO CARRIER\r\n";
                send_response(client_fd, pending, response);
                pending.clear();
            }
            continue;
        }

        ssize_t n = read(client_fd, buf, sizeof(buf));
        if (n <= 0) {
            if (n == 0) {
                printf("modem_sim: client disconnected\n");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("modem_sim: read error");
            }
            break;
        }

        pending.append(buf, n);

        // Handle "+++" escape sequence (no \r)
        size_t ppp_pos = pending.find("+++");
        if (ppp_pos != std::string::npos && pending.find('\r') == std::string::npos) {
            std::string response = "NO CARRIER\r\n";
            send_response(client_fd, "+++", response);
            pending.erase(0, ppp_pos + 3);
            continue;
        }

        size_t pos;
        while ((pos = pending.find('\r')) != std::string::npos) {
            std::string cmd = pending.substr(0, pos + 1);
            pending.erase(0, pos + 1);

            std::string response = process_at_command(cmd);
            send_response(client_fd, cmd, response);
        }
    }
}

static void usage(const char *prog)
{
    printf("Usage: %s [port] [batch_size] [batch_delay_ms]\n"
           "  port           TCP listen port (default: 10000)\n"
           "  batch_size     reply chunk size in bytes, 0=whole (default: 0)\n"
           "  batch_delay_ms delay between chunks in ms (default: 1)\n", prog);
}

int main(int argc, char *argv[])
{
    int port = 10000;
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        port = atoi(argv[1]);
    }
    if (argc > 2) {
        batch_size = atoi(argv[2]);
    }
    if (argc > 3) {
        batch_delay_ms = atoi(argv[3]);
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("modem_sim: socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("modem_sim: bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("modem_sim: listen");
        close(server_fd);
        return 1;
    }

    printf("modem_sim: listening on 127.0.0.1:%d (batch_size=%d, batch_delay=%dms)\n",
           port, batch_size, batch_delay_ms);
    fflush(stdout);

    while (running) {
        struct sockaddr_in client_addr = {};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("modem_sim: accept");
            break;
        }

        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    printf("modem_sim: shutdown\n");
    return 0;
}
