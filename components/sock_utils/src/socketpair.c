/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <fcntl.h>
#include "lwip/sockets.h"
#include "socketpair.h"
#include "esp_check.h"

#define INVALID_SOCKET (-1)

static const char *TAG = "socket_helpers";

static int set_nonblocking(int sock)
{
    int opt;
    opt = fcntl(sock, F_GETFL, 0);
    if (opt == -1) {
        return -1;
    }
    if (fcntl(sock, F_SETFL, opt | O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}

int socketpair(int domain, int type, int protocol, int sv[2])
{
    struct sockaddr_storage ss;
    struct sockaddr_in *sa = (struct sockaddr_in *)&ss;
    socklen_t ss_len = sizeof(struct sockaddr_in);
    int fd1 = INVALID_SOCKET;
    int fd2 = INVALID_SOCKET;
    int listenfd = INVALID_SOCKET;
    int ret = 0; // Success

    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa->sin_port = 0;
    listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ESP_GOTO_ON_FALSE(listenfd != INVALID_SOCKET, -1, err, TAG, "Cannot create listening socket");
    ESP_GOTO_ON_FALSE(listen(listenfd, 1) == 0, -1, err, TAG, "Failed to listen");

    memset(&ss, 0, sizeof(ss));
    ss_len = sizeof(ss);
    ESP_GOTO_ON_FALSE(getsockname(listenfd, (struct sockaddr *)&ss, &ss_len) >= 0, -1, err, TAG, "getsockname failed");

    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    fd1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ESP_GOTO_ON_FALSE(fd1 != INVALID_SOCKET, -1, err, TAG, "Cannot create read side socket");
    ESP_GOTO_ON_FALSE(set_nonblocking(fd1) == 0, -1, err, TAG, "Failed to set socket to nonblocking mode");
    if (connect(fd1, (struct sockaddr *)&ss, ss_len) < 0) {
        ESP_GOTO_ON_FALSE(errno == EINPROGRESS || errno == EWOULDBLOCK, -1, err, TAG, "Failed to connect fd1");
    }
    fd2 = accept(listenfd, NULL, 0);
    if (fd2 == -1) {
        ESP_GOTO_ON_FALSE(errno == EINPROGRESS || errno == EWOULDBLOCK, -1, err, TAG, "Failed to accept fd2");
    }
    ESP_GOTO_ON_FALSE(set_nonblocking(fd2) == 0, -1, err, TAG, "Failed to set socket to nonblocking mode");

    close(listenfd);
    sv[0] = fd1;
    sv[1] = fd2;
    return ret;

err:
    if (listenfd != INVALID_SOCKET) {
        close(listenfd);
    }
    if (fd1 != INVALID_SOCKET) {
        close(fd1);
    }
    if (fd2 != INVALID_SOCKET) {
        close(fd2);
    }
    return ret;
}

int pipe(int pipefd[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) == -1) {
        return -1;
    }
    return 0;
}
