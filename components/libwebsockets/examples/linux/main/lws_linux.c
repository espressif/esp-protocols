/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*
 * libwebsockets on the IDF linux target: lws is built for the host (plat/unix)
 * and linked against IDF's mbedtls. Creating a client context initialises the
 * mbedtls TLS backend, so this checks the build, the link and TLS bring-up
 * without needing a network.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>

/* Exits instead of returning, so the process ends and CI sees the status. */
void app_main(void)
{
    struct lws_context_creation_info info;
    struct lws_context *context;

    lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE, NULL);
    printf("libwebsockets %s\n", lws_get_library_version());

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (context == NULL) {
        printf("lws_create_context() failed\n");
        exit(1);
    }
    printf("lws context created with mbedtls TLS\n");

    lws_context_destroy(context);
    exit(0);
}
