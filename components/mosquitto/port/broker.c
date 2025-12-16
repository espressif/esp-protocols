/*
 * SPDX-FileCopyrightText: 2024 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */
#include "mosquitto.h"
#include "mosquitto_broker_internal.h"
#include "memory_mosq.h"
#include "mosq_broker.h"
#include <time.h>
#include <sys/time.h>

static struct mosquitto__listener_sock *listensock = NULL;
static int listensock_count = 0;
static int listensock_index = 0;
extern int run;

static int listeners__start_single_mqtt(struct mosquitto__listener *listener)
{
    int i;
    struct mosquitto__listener_sock *listensock_new;

    if (net__socket_listen(listener)) {
        return 1;
    }
    listensock_count += listener->sock_count;
    listensock_new = mosquitto__realloc(listensock, sizeof(struct mosquitto__listener_sock) * (size_t)listensock_count);
    if (!listensock_new) {
        return 1;
    }
    listensock = listensock_new;

    for (i = 0; i < listener->sock_count; i++) {
        if (listener->socks[i] == INVALID_SOCKET) {
            return 1;
        }
        listensock[listensock_index].sock = listener->socks[i];
        listensock[listensock_index].listener = listener;
#ifdef WITH_EPOLL
        listensock[listensock_index].ident = id_listener;
#endif
        listensock_index++;
    }
    return MOSQ_ERR_SUCCESS;
}

static int listeners__add_local(const char *host, uint16_t port)
{
    struct mosquitto__listener *listeners;
    listeners = mosquitto__realloc(db.config->listeners, sizeof(struct mosquitto__listener));
    if (listeners == NULL) {
        return MOSQ_ERR_NOMEM;
    }
    memset(listeners, 0, sizeof(struct mosquitto__listener));
    db.config->listener_count = 0;
    db.config->listeners = listeners;

    listeners = db.config->listeners;

    listener__set_defaults(&listeners[db.config->listener_count]);
    listeners[db.config->listener_count].security_options.allow_anonymous = true;
    listeners[db.config->listener_count].port = port;
    listeners[db.config->listener_count].host = mosquitto__strdup(host);
    if (listeners[db.config->listener_count].host == NULL) {
        return MOSQ_ERR_NOMEM;
    }
    if (listeners__start_single_mqtt(&listeners[db.config->listener_count])) {
        mosquitto__free(listeners[db.config->listener_count].host);
        listeners[db.config->listener_count].host = NULL;
        return MOSQ_ERR_UNKNOWN;
    }
    db.config->listener_count++;
    return MOSQ_ERR_SUCCESS;
}

static void listeners__stop(void)
{
    int i;

    for (i = 0; i < db.config->listener_count; i++) {
#ifdef WITH_WEBSOCKETS
        if (db.config->listeners[i].ws_context) {
            lws_context_destroy(db.config->listeners[i].ws_context);
        }
        mosquitto__free(db.config->listeners[i].ws_protocol);
#endif
    }

    for (i = 0; i < listensock_count; i++) {
        if (listensock[i].sock != INVALID_SOCKET) {
            COMPAT_CLOSE(listensock[i].sock);
        }
    }
    mosquitto__free(listensock);
}

void net__set_tls_config(esp_tls_cfg_server_t *config);

void mosq_broker_stop(void)
{
    run = 0;
}

extern mosq_message_cb_t g_mosq_message_callback;
extern mosq_connect_cb_t g_mosq_connect_callback;

int mosq_broker_run(struct mosq_broker_config *broker_config)
{

    struct mosquitto__config config;
#ifdef WITH_BRIDGE
    int i;
#endif
    int rc;
    struct timeval tv;
    struct mosquitto *ctxt, *ctxt_tmp;

    gettimeofday(&tv, NULL);

    memset(&db, 0, sizeof(struct mosquitto_db));
    db.now_s = mosquitto_time();
    db.now_real_s = time(NULL);

    net__broker_init();

    config__init(&config);

    if (broker_config->tls_cfg) {
        net__set_tls_config(broker_config->tls_cfg);
    }
    if (broker_config->handle_message_cb) {
        g_mosq_message_callback = broker_config->handle_message_cb;
    }
    if (broker_config->handle_connect_cb) {
        g_mosq_connect_callback = broker_config->handle_connect_cb;
    }

    db.config = &config;

    rc = db__open(&config);
    if (rc != MOSQ_ERR_SUCCESS) {
        log__printf(NULL, MOSQ_LOG_ERR, "Error: Couldn't open database.");
        return rc;
    }

    if (log__init(&config)) {
        rc = 1;
        return rc;
    }
    log__printf(NULL, MOSQ_LOG_INFO, "mosquitto version %s starting", VERSION);
    if (db.config_file) {
        log__printf(NULL, MOSQ_LOG_INFO, "Config loaded from %s.", db.config_file);
    } else {
        log__printf(NULL, MOSQ_LOG_INFO, "Using default config.");
    }

    if (listeners__add_local(broker_config->host, broker_config->port)) {
        return 1;
    }

    rc = mux__init(listensock, listensock_count);
    if (rc) {
        return rc;
    }

#ifdef WITH_BRIDGE
    bridge__start_all();
#endif

    log__printf(NULL, MOSQ_LOG_INFO, "mosquitto version %s running", VERSION);

    run = 1;
    rc = mosquitto_main_loop(listensock, listensock_count);

    log__printf(NULL, MOSQ_LOG_INFO, "mosquitto version %s terminating", VERSION);

    HASH_ITER(hh_id, db.contexts_by_id, ctxt, ctxt_tmp) {
        context__send_will(ctxt);
    }
    will_delay__send_all();

#ifdef WITH_PERSISTENCE
    persist__backup(true);
#endif
    session_expiry__remove_all();

    listeners__stop();

    HASH_ITER(hh_id, db.contexts_by_id, ctxt, ctxt_tmp) {
#ifdef WITH_WEBSOCKETS
        if (!ctxt->wsi)
#endif
        {
            context__cleanup(ctxt, true);
        }
    }
    HASH_ITER(hh_sock, db.contexts_by_sock, ctxt, ctxt_tmp) {
        context__cleanup(ctxt, true);
    }
#ifdef WITH_BRIDGE
    for (i = 0; i < db.bridge_count; i++) {
        if (db.bridges[i]) {
            context__cleanup(db.bridges[i], true);
        }
    }
    mosquitto__free(db.bridges);
#endif
    context__free_disused();

    db__close();

    mosquitto_security_module_cleanup();

    log__close(&config);
    config__cleanup(db.config);
    net__broker_cleanup();

    return rc;
}
