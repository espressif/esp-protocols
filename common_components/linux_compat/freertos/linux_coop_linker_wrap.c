/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esp_private/freertos_linux_coop_syscalls.h"

static void __attribute__((constructor)) freertos_linux_coop_linker_wrap_init(void)
{
    freertos_linux_coop_syscalls_init();
}

int __wrap_socket(int domain, int type, int protocol)
{
    return freertos_linux_coop_socket(domain, type, protocol);
}

int __wrap_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return freertos_linux_coop_accept(sockfd, addr, addrlen);
}

int __wrap_fcntl(int fd, int cmd, ...)
{
    va_list args;
    int arg = 0;

    /* Only pull va_arg for cmds that take an int arg (not F_GETFD/F_GETFL/…). */
    switch (cmd) {
    case F_DUPFD:
#ifdef F_DUPFD_CLOEXEC
    case F_DUPFD_CLOEXEC:
#endif
    case F_SETFD:
    case F_SETFL:
#ifdef F_SETOWN
    case F_SETOWN:
#endif
#ifdef F_SETSIG
    case F_SETSIG:
#endif
#ifdef F_SETLEASE
    case F_SETLEASE:
#endif
#ifdef F_NOTIFY
    case F_NOTIFY:
#endif
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ:
#endif
#ifdef F_ADD_SEALS
    case F_ADD_SEALS:
#endif
        va_start(args, cmd);
        arg = va_arg(args, int);
        va_end(args);
        break;
    default:
        break;
    }
    return freertos_linux_coop_fcntl(fd, cmd, arg);
}

int __wrap_close(int fd)
{
    return freertos_linux_coop_close(fd);
}

ssize_t __wrap_send(int sockfd, const void *buf, size_t len, int flags)
{
    return freertos_linux_coop_send(sockfd, buf, len, flags);
}

ssize_t __wrap_read(int fd, void *buf, size_t count)
{
    return freertos_linux_coop_read(fd, buf, count);
}

int __wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    return freertos_linux_coop_poll(fds, nfds, timeout);
}
