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

static void send_response(int fd, const std::string &cmd, const std::string &response)
{
    printf("modem_sim: rx [%zu]: ", cmd.size());
    for (auto c : cmd) {
        printf(c >= 0x20 ? "%c" : "\\x%02x", (unsigned char)c);
    }
    printf("\n");
    printf("modem_sim: tx [%zu]: ", response.size());
    for (auto c : response) {
        printf(c >= 0x20 ? "%c" : "\\x%02x", (unsigned char)c);
    }
    printf("\n");
    fflush(stdout);

    size_t total = 0;
    while (total < response.size()) {
        ssize_t sent = write(fd, response.c_str() + total, response.size() - total);
        if (sent < 0) {
            perror("modem_sim: write error");
            return;
        }
        total += sent;
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

int main(int argc, char *argv[])
{
    int port = 10000;
    if (argc > 1) {
        port = atoi(argv[1]);
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

    printf("modem_sim: listening on 127.0.0.1:%d\n", port);
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
