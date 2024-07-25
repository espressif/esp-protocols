/*
 * SPDX-FileCopyrightText: 2009-2020 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 *
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */

/*
All rights reserved. This program and the accompanying materials
are made available under the terms of the Eclipse Public License 2.0
and Eclipse Distribution License v1.0 which accompany this distribution.

The Eclipse Public License is available at
   https://www.eclipse.org/legal/epl-2.0/
and the Eclipse Distribution License is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

Contributors:
   Roger Light - initial implementation and documentation.
   Espressif Systems (Shanghai) CO LTD - added support for ESP-TLS based connections
*/

#include "config.h"

#  include <arpa/inet.h>
#  include <ifaddrs.h>
#  include <netdb.h>
#  include <netinet/tcp.h>
#  include <strings.h>
#  include <sys/socket.h>
#  include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#include "mosquitto_broker_internal.h"
#include "mqtt_protocol.h"
#include "memory_mosq.h"
#include "misc_mosq.h"
#include "net_mosq.h"
#include "util_mosq.h"
#include "esp_tls.h"

#include "sys_tree.h"

#define MAX_CONNECTIONS (64)

struct esp_tls_context {
    int sock;
    esp_tls_t *tls;
};

static struct esp_tls_context tls_ctx[MAX_CONNECTIONS];
static esp_tls_cfg_server_t *tls_cfg;

void net__set_tls_config(esp_tls_cfg_server_t *config)
{
    if (config) {
        tls_cfg = mosquitto__calloc(1, sizeof(esp_tls_cfg_server_t));
        if (tls_cfg) {
            memcpy(tls_cfg, config, sizeof(esp_tls_cfg_server_t));
        } else {
            log__printf(NULL, MOSQ_LOG_ERR, "Unable to allocate ESP-TLS configuration structure, continuing with plain TCP transport only");
        }
    }
}

void net__broker_init(void)
{
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        tls_ctx[i].sock = INVALID_SOCKET;
        tls_ctx[i].tls = NULL;
    }
    net__init();
}


void net__broker_cleanup(void)
{
    net__cleanup();
    mosquitto__free(tls_cfg);
    tls_cfg = NULL;
}


static void net__print_error(unsigned int log, const char *format_str)
{
    char *buf;
    buf = strerror(errno);
    log__printf(NULL, log, format_str, buf);
}


struct mosquitto *net__socket_accept(struct mosquitto__listener_sock *listensock)
{
    mosq_sock_t new_sock = INVALID_SOCKET;
    struct mosquitto *new_context;

    new_sock = accept(listensock->sock, NULL, 0);
    if (new_sock == INVALID_SOCKET) {
        log__printf(NULL, MOSQ_LOG_ERR,
                    "Unable to accept new connection, system socket count has been exceeded. Try increasing \"ulimit -n\" or equivalent.");
        return NULL;
    }

    if (tls_cfg) {
        // Finds first free spot in the context array
        int ctx;
        for (ctx = 0; ctx < MAX_CONNECTIONS; ++ctx) {
            if (tls_ctx[ctx].sock == INVALID_SOCKET) {
                tls_ctx[ctx].sock = new_sock;
                tls_ctx[ctx].tls = esp_tls_init();
                if (!tls_ctx[ctx].tls) {
                    log__printf(NULL, MOSQ_LOG_ERR, "Faled to create a new ESP-TLS context");
                    return NULL;
                }
                break;
            }
        }
        if (ctx >= MAX_CONNECTIONS) {
            log__printf(NULL, MOSQ_LOG_ERR, "Unable to create new ESP-TLS connection. Try increasing \"MAX_CONNECTIONS\"");
            return NULL;
        }
        int ret = esp_tls_server_session_create(tls_cfg, new_sock, tls_ctx[ctx].tls);
        if (ret != 0) {
            log__printf(NULL, MOSQ_LOG_ERR, "Unable to create new ESP-TLS session");
            return NULL;
        }
    }

    G_SOCKET_CONNECTIONS_INC();

    if (net__socket_nonblock(&new_sock)) {
        return NULL;
    }

    if (db.config->set_tcp_nodelay) {
        int flag = 1;
        if (setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) != 0) {
            log__printf(NULL, MOSQ_LOG_WARNING, "Warning: Unable to set TCP_NODELAY.");
        }
    }

    new_context = context__init(new_sock);
    if (!new_context) {
        COMPAT_CLOSE(new_sock);
        return NULL;
    }
    new_context->listener = listensock->listener;
    if (!new_context->listener) {
        context__cleanup(new_context, true);
        return NULL;
    }
    new_context->listener->client_count++;

    if (new_context->listener->max_connections > 0 && new_context->listener->client_count > new_context->listener->max_connections) {
        if (db.config->connection_messages == true) {
            log__printf(NULL, MOSQ_LOG_NOTICE, "Client connection from %s denied: max_connections exceeded.", new_context->address);
        }
        context__cleanup(new_context, true);
        return NULL;
    }

    if (db.config->connection_messages == true) {
        log__printf(NULL, MOSQ_LOG_NOTICE, "New connection from %s:%d on port %d.",
                    new_context->address, new_context->remote_port, new_context->listener->port);
    }

    return new_context;
}

int net__load_certificates(struct mosquitto__listener *listener)
{
    return MOSQ_ERR_SUCCESS;
}


int net__tls_load_verify(struct mosquitto__listener *listener)
{
    return net__load_certificates(listener);
}


static int net__socket_listen_tcp(struct mosquitto__listener *listener)
{
    mosq_sock_t sock = INVALID_SOCKET;
    struct addrinfo hints;
    struct addrinfo *ainfo, *rp;
    char service[10];
    int rc;
    int ss_opt = 1;

    if (!listener) {
        return MOSQ_ERR_INVAL;
    }

    snprintf(service, 10, "%d", listener->port);
    memset(&hints, 0, sizeof(struct addrinfo));
    if (listener->socket_domain) {
        hints.ai_family = listener->socket_domain;
    } else {
        hints.ai_family = AF_UNSPEC;
    }
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(listener->host, service, &hints, &ainfo);
    if (rc) {
        log__printf(NULL, MOSQ_LOG_ERR, "Error creating listener: %s.", gai_strerror(rc));
        return INVALID_SOCKET;
    }

    listener->sock_count = 0;
    listener->socks = NULL;

    for (rp = ainfo; rp; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            log__printf(NULL, MOSQ_LOG_INFO, "Opening ipv4 listen socket on port %d.", ntohs(((struct sockaddr_in *)rp->ai_addr)->sin_port));
        } else if (rp->ai_family == AF_INET6) {
            log__printf(NULL, MOSQ_LOG_INFO, "Opening ipv6 listen socket on port %d.", ntohs(((struct sockaddr_in6 *)rp->ai_addr)->sin6_port));
        } else {
            continue;
        }

        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == INVALID_SOCKET) {
            net__print_error(MOSQ_LOG_WARNING, "Warning: %s");
            continue;
        }
        listener->sock_count++;
        listener->socks = mosquitto__realloc(listener->socks, sizeof(mosq_sock_t) * (size_t)listener->sock_count);
        if (!listener->socks) {
            log__printf(NULL, MOSQ_LOG_ERR, "Error: Out of memory.");
            freeaddrinfo(ainfo);
            COMPAT_CLOSE(sock);
            return MOSQ_ERR_NOMEM;
        }
        listener->socks[listener->sock_count - 1] = sock;

#ifndef WIN32
        ss_opt = 1;
        /* Unimportant if this fails */
        (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ss_opt, sizeof(ss_opt));
#endif
#ifdef IPV6_V6ONLY
        ss_opt = 1;
        (void)setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &ss_opt, sizeof(ss_opt));
#endif

        if (net__socket_nonblock(&sock)) {
            freeaddrinfo(ainfo);
            mosquitto__free(listener->socks);
            return 1;
        }

        if (listener->bind_interface) {
            log__printf(NULL, MOSQ_LOG_ERR, "Error: listener->bind_interface is not supported");
            return 1;
        }

        if (bind(sock, rp->ai_addr, rp->ai_addrlen) == -1) {
            net__print_error(MOSQ_LOG_ERR, "Error: %s");
            COMPAT_CLOSE(sock);
            freeaddrinfo(ainfo);
            mosquitto__free(listener->socks);
            return 1;
        }

        if (listen(sock, 100) == -1) {
            net__print_error(MOSQ_LOG_ERR, "Error: %s");
            freeaddrinfo(ainfo);
            COMPAT_CLOSE(sock);
            mosquitto__free(listener->socks);
            return 1;
        }
    }
    freeaddrinfo(ainfo);

    if (listener->bind_interface) {
        mosquitto__free(listener->socks);
        return 1;
    }

    return 0;
}

/* Creates a socket and listens on port 'port'.
 * Returns 1 on failure
 * Returns 0 on success.
 */
int net__socket_listen(struct mosquitto__listener *listener)
{
    int rc;

    if (!listener) {
        return MOSQ_ERR_INVAL;
    }

    rc = net__socket_listen_tcp(listener);
    if (rc) {
        return rc;
    }

    /* We need to have at least one working socket. */
    if (listener->sock_count > 0) {
        return 0;
    } else {
        return 1;
    }
}

int net__socket_get_address(mosq_sock_t sock, char *buf, size_t len, uint16_t *remote_port)
{
    struct sockaddr_storage addr;
    socklen_t addrlen;

    memset(&addr, 0, sizeof(struct sockaddr_storage));
    addrlen = sizeof(addr);
    if (!getpeername(sock, (struct sockaddr *)&addr, &addrlen)) {
        if (addr.ss_family == AF_INET) {
            if (remote_port) {
                *remote_port = ntohs(((struct sockaddr_in *)&addr)->sin_port);
            }
            if (inet_ntop(AF_INET, &((struct sockaddr_in *)&addr)->sin_addr.s_addr, buf, (socklen_t)len)) {
                return 0;
            }
        } else if (addr.ss_family == AF_INET6) {
            if (remote_port) {
                *remote_port = ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
            }
            if (inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&addr)->sin6_addr.s6_addr, buf, (socklen_t)len)) {
                return 0;
            }
        }
    }
    return 1;
}

// -----------------------------------

ssize_t net__read(struct mosquitto *mosq, void *buf, size_t count)
{
    assert(mosq);
    errno = 0;
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (tls_ctx[i].sock == mosq->sock) {
            return esp_tls_conn_read(tls_ctx[i].tls, buf, count);
        }
    }
    return read(mosq->sock, buf, count);
}

ssize_t net__write(struct mosquitto *mosq, const void *buf, size_t count)
{
    assert(mosq);
    errno = 0;
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (tls_ctx[i].sock == mosq->sock) {
            return esp_tls_conn_write(tls_ctx[i].tls, buf, count);
        }
    }
    return send(mosq->sock, buf, count, MSG_NOSIGNAL);
}


int net__socket_nonblock(mosq_sock_t *sock)
{
    int opt;
    /* Set non-blocking */
    opt = fcntl(*sock, F_GETFL, 0);
    if (opt == -1) {
        COMPAT_CLOSE(*sock);
        *sock = INVALID_SOCKET;
        return MOSQ_ERR_ERRNO;
    }
    if (fcntl(*sock, F_SETFL, opt | O_NONBLOCK) == -1) {
        /* If either fcntl fails, don't want to allow this client to connect. */
        COMPAT_CLOSE(*sock);
        *sock = INVALID_SOCKET;
        return MOSQ_ERR_ERRNO;
    }
    return MOSQ_ERR_SUCCESS;
}


/* Close a socket associated with a context and set it to -1.
 * Returns 1 on failure (context is NULL)
 * Returns 0 on success.
 */
int net__socket_close(struct mosquitto *mosq)
{
    int rc = 0;
#ifdef WITH_BROKER
    struct mosquitto *mosq_found;
#endif

    assert(mosq);
#ifdef WITH_TLS
#ifdef WITH_WEBSOCKETS
    if (!mosq->wsi)
#endif
    {
        if (mosq->ssl) {
            if (!SSL_in_init(mosq->ssl)) {
                SSL_shutdown(mosq->ssl);
            }
            SSL_free(mosq->ssl);
            mosq->ssl = NULL;
        }
    }
#endif

#ifdef WITH_WEBSOCKETS
    if (mosq->wsi) {
        if (mosq->state != mosq_cs_disconnecting) {
            mosquitto__set_state(mosq, mosq_cs_disconnect_ws);
        }
        lws_callback_on_writable(mosq->wsi);
    } else
#endif
    {
        if (mosq->sock != INVALID_SOCKET) {
#ifdef WITH_BROKER
            HASH_FIND(hh_sock, db.contexts_by_sock, &mosq->sock, sizeof(mosq->sock), mosq_found);
            if (mosq_found) {
                HASH_DELETE(hh_sock, db.contexts_by_sock, mosq_found);
            }
#endif
            rc = COMPAT_CLOSE(mosq->sock);
            // Finds first free spot in the context array
            for (int i = 0; i < MAX_CONNECTIONS; ++i) {
                if (tls_ctx[i].sock == mosq->sock) {
                    tls_ctx[i].sock = INVALID_SOCKET;
                    esp_tls_server_session_delete(tls_ctx[i].tls);
                    break;
                }
            }

            mosq->sock = INVALID_SOCKET;
        }
    }

#ifdef WITH_BROKER
    if (mosq->listener) {
        mosq->listener->client_count--;
        mosq->listener = NULL;
    }
#endif
    return rc;
}

int net__init(void)
{
    return MOSQ_ERR_SUCCESS;
}

void net__cleanup(void)
{
}
