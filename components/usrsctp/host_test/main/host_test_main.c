/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host (IDF Linux target) smoke + socket-lifecycle test for usrsctp.
 *
 * - usrsctp_init() with a no-op conn_output callback.
 * - usrsctp_socket() to allocate a userspace SCTP endpoint.
 * - usrsctp_close() + usrsctp_finish() shut down cleanly.
 *
 * A deeper loopback association test (two endpoints, real 4-way handshake)
 * is tracked separately — it needs the upstream usrsctp timer thread
 * driving conninput, which isn't trivial to spin up inside an
 * IDF-Linux-target single-threaded entry point without leaking
 * threads at exit. Out of scope for the registry submission.
 */
#include <stdio.h>
#include <stdlib.h>
#include "usrsctp.h"

static int rc = 0;

#define CHECK(_cond, _msg)                                  \
    do {                                                    \
        if (!(_cond)) {                                     \
            fprintf(stderr, "FAIL: %s\n", _msg);            \
            rc = 1;                                         \
        }                                                   \
    } while (0)

static int dummy_conn_output(void *addr, void *buf, size_t length,
                             uint8_t tos, uint8_t set_df)
{
    (void)addr; (void)buf; (void)length; (void)tos; (void)set_df;
    return 0;
}

static int run_smoke(void)
{
    printf("usrsctp host_test: starting\n");

    usrsctp_init(0, dummy_conn_output, NULL);

    struct socket *sock = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                                         NULL, NULL, 0, NULL);
    CHECK(sock != NULL, "usrsctp_socket returned NULL");

    if (sock != NULL) {
        usrsctp_close(sock);
    }

    /* finish() returns the number of remaining endpoints — 0 means clean. */
    int remaining = usrsctp_finish();
    CHECK(remaining == 0, "usrsctp_finish left endpoints behind");

    if (rc == 0) {
        printf("usrsctp host_test: PASS\n");
    } else {
        printf("usrsctp host_test: FAIL\n");
    }
    return rc;
}

void app_main(void)
{
    exit(run_smoke());
}
