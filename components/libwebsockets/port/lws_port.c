/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdio.h"
#include <libwebsockets.h>

/*
 * External function prototype for the wrapped  'mbedtls_ssl_handshake_step'.
 * The "real" function is not being called, this prototype is just to improve
 * the code readability.
 */
extern int __real_mbedtls_ssl_handshake_step(mbedtls_ssl_context *ssl);

int  __wrap_mbedtls_ssl_handshake_step(mbedtls_ssl_context *ssl)
{
    int ret = 0;

    while (ssl->MBEDTLS_PRIVATE(state) != MBEDTLS_SSL_HANDSHAKE_OVER) {
        ret = __real_mbedtls_ssl_handshake_step(ssl);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }

        if (ret != 0) {
            break;
        }
    }

    return ret;
}

/*
 * External function prototype for the wrapped 'lws_adopt_descriptor_vhost'.
 * The "real" function is not being called, this prototype is just to improve
 * the code readability.
 */
extern struct lws *__real_lws_adopt_descriptor_vhost(struct lws_vhost *vh, lws_adoption_type type, lws_sock_file_fd_type fd, const char *vh_prot_name, struct lws *parent);

struct lws *__wrap_lws_adopt_descriptor_vhost(struct lws_vhost *vh, lws_adoption_type type, lws_sock_file_fd_type fd, const char *vh_prot_name, struct lws *parent)
{
    lws_adopt_desc_t info;
    char nullstr[] = "(null)";
    memset(&info, 0, sizeof(info));

    info.vh = vh;
    info.type = type;
    info.fd = fd;
    info.vh_prot_name = vh_prot_name;
    info.parent = parent;
    info.fi_wsi_name = nullstr;

    return lws_adopt_descriptor_vhost_via_info(&info);
}
