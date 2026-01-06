/*
 * SPDX-FileCopyrightText: 2024 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * SPDX-FileContributor: 2024-2025 Espressif Systems (Shanghai) CO LTD
 */
#include "mosquitto_internal.h"
#include "mosquitto_broker.h"
#include "memory_mosq.h"
#include "mqtt_protocol.h"
#include "send_mosq.h"
#include "util_mosq.h"
#include "utlist.h"
#include "lib_load.h"
#include "syslog.h"
#include <limits.h>
#include "sdkconfig.h"


void config__init(struct mosquitto__config *config)
{
    memset(config, 0, sizeof(struct mosquitto__config));
    for (int i = 0; i < config->listener_count; i++) {
        mosquitto__free(config->listeners[i].security_options.acl_file);
        config->listeners[i].security_options.acl_file = NULL;

        mosquitto__free(config->listeners[i].security_options.password_file);
        config->listeners[i].security_options.password_file = NULL;

        mosquitto__free(config->listeners[i].security_options.psk_file);
        config->listeners[i].security_options.psk_file = NULL;

        config->listeners[i].security_options.allow_anonymous = -1;
        config->listeners[i].security_options.allow_zero_length_clientid = true;
        config->listeners[i].security_options.auto_id_prefix = NULL;
        config->listeners[i].security_options.auto_id_prefix_len = 0;
    }

    config->local_only = true;
    config->allow_duplicate_messages = false;

    mosquitto__free(config->security_options.acl_file);
    config->security_options.acl_file = NULL;

    config->security_options.allow_anonymous = -1;
    config->security_options.allow_zero_length_clientid = true;
    config->security_options.auto_id_prefix = NULL;
    config->security_options.auto_id_prefix_len = 0;

    mosquitto__free(config->security_options.password_file);
    config->security_options.password_file = NULL;

    mosquitto__free(config->security_options.psk_file);
    config->security_options.psk_file = NULL;

    config->autosave_interval = 1800;
    config->autosave_on_changes = false;
    mosquitto__free(config->clientid_prefixes);
    config->connection_messages = true;
    config->clientid_prefixes = NULL;
    config->per_listener_settings = false;
    if (config->log_fptr) {
        fclose(config->log_fptr);
        config->log_fptr = NULL;
    }
    mosquitto__free(config->log_file);
    config->log_file = NULL;

    config->log_facility = LOG_DAEMON;
    config->log_dest = MQTT3_LOG_STDERR | MQTT3_LOG_TOPIC;
    if (db.verbose) {
        config->log_type = UINT_MAX;
    } else {
        config->log_type = MOSQ_LOG_ERR | MOSQ_LOG_WARNING | MOSQ_LOG_NOTICE | MOSQ_LOG_INFO;
    }

    config->log_timestamp = true;
    mosquitto__free(config->log_timestamp_format);
    config->log_timestamp_format = NULL;
    config->max_keepalive = 0;
    config->max_packet_size = 0;
    config->max_inflight_messages = 20;
    config->max_queued_messages = 1000;
    config->max_inflight_bytes = 0;
    config->max_queued_bytes = 0;
    config->persistence = false;
    mosquitto__free(config->persistence_location);
    config->persistence_location = NULL;
    mosquitto__free(config->persistence_file);
    config->persistence_file = NULL;
    config->persistent_client_expiration = 0;
    config->queue_qos0_messages = false;
    config->retain_available = true;
    config->set_tcp_nodelay = false;
#if defined(WITH_SYS_TREE)
    config->sys_interval = CONFIG_MOSQ_SYS_UPDATE_INTERVAL;
#endif
    config->upgrade_outgoing_qos = false;

    config->daemon = false;
    memset(&config->default_listener, 0, sizeof(struct mosquitto__listener));
    listener__set_defaults(&config->default_listener);
}

int config__read(struct mosquitto__config *config, bool reload)
{
    return MOSQ_ERR_SUCCESS;
}

void config__cleanup(struct mosquitto__config *config)
{
    int i;
#ifdef WITH_BRIDGE
    int j;
#endif

    mosquitto__free(config->clientid_prefixes);
    mosquitto__free(config->persistence_location);
    mosquitto__free(config->persistence_file);
    mosquitto__free(config->persistence_filepath);
    mosquitto__free(config->security_options.auto_id_prefix);
    mosquitto__free(config->security_options.acl_file);
    mosquitto__free(config->security_options.password_file);
    mosquitto__free(config->security_options.psk_file);
    mosquitto__free(config->pid_file);
    mosquitto__free(config->user);
    mosquitto__free(config->log_timestamp_format);
    if (config->listeners) {
        for (i = 0; i < config->listener_count; i++) {
            mosquitto__free(config->listeners[i].host);
            mosquitto__free(config->listeners[i].bind_interface);
            mosquitto__free(config->listeners[i].mount_point);
            mosquitto__free(config->listeners[i].socks);
            mosquitto__free(config->listeners[i].security_options.auto_id_prefix);
            mosquitto__free(config->listeners[i].security_options.acl_file);
            mosquitto__free(config->listeners[i].security_options.password_file);
            mosquitto__free(config->listeners[i].security_options.psk_file);
#ifdef WITH_TLS
            mosquitto__free(config->listeners[i].cafile);
            mosquitto__free(config->listeners[i].capath);
            mosquitto__free(config->listeners[i].certfile);
            mosquitto__free(config->listeners[i].keyfile);
            mosquitto__free(config->listeners[i].ciphers);
            mosquitto__free(config->listeners[i].ciphers_tls13);
            mosquitto__free(config->listeners[i].psk_hint);
            mosquitto__free(config->listeners[i].crlfile);
            mosquitto__free(config->listeners[i].dhparamfile);
            mosquitto__free(config->listeners[i].tls_version);
            mosquitto__free(config->listeners[i].tls_engine);
            mosquitto__free(config->listeners[i].tls_engine_kpass_sha1);
#ifdef WITH_WEBSOCKETS
            if (!config->listeners[i].ws_context) /* libwebsockets frees its own SSL_CTX */
#endif
            {
                SSL_CTX_free(config->listeners[i].ssl_ctx);
            }
#endif
#ifdef WITH_WEBSOCKETS
            mosquitto__free(config->listeners[i].http_dir);
#endif

        }
        mosquitto__free(config->listeners);
    }
#ifdef WITH_BRIDGE
    if (config->bridges) {
        for (i = 0; i < config->bridge_count; i++) {
            mosquitto__free(config->bridges[i].name);
            if (config->bridges[i].addresses) {
                for (j = 0; j < config->bridges[i].address_count; j++) {
                    mosquitto__free(config->bridges[i].addresses[j].address);
                }
                mosquitto__free(config->bridges[i].addresses);
            }
            mosquitto__free(config->bridges[i].remote_clientid);
            mosquitto__free(config->bridges[i].remote_username);
            mosquitto__free(config->bridges[i].remote_password);
            mosquitto__free(config->bridges[i].local_clientid);
            mosquitto__free(config->bridges[i].local_username);
            mosquitto__free(config->bridges[i].local_password);
            if (config->bridges[i].topics) {
                for (j = 0; j < config->bridges[i].topic_count; j++) {
                    mosquitto__free(config->bridges[i].topics[j].topic);
                    mosquitto__free(config->bridges[i].topics[j].local_prefix);
                    mosquitto__free(config->bridges[i].topics[j].remote_prefix);
                    mosquitto__free(config->bridges[i].topics[j].local_topic);
                    mosquitto__free(config->bridges[i].topics[j].remote_topic);
                }
                mosquitto__free(config->bridges[i].topics);
            }
            mosquitto__free(config->bridges[i].notification_topic);
#ifdef WITH_TLS
            mosquitto__free(config->bridges[i].tls_version);
            mosquitto__free(config->bridges[i].tls_cafile);
            mosquitto__free(config->bridges[i].tls_alpn);
#ifdef FINAL_WITH_TLS_PSK
            mosquitto__free(config->bridges[i].tls_psk_identity);
            mosquitto__free(config->bridges[i].tls_psk);
#endif
#endif
        }
        mosquitto__free(config->bridges);
    }
#endif

    if (config->log_fptr) {
        fclose(config->log_fptr);
        config->log_fptr = NULL;
    }
    if (config->log_file) {
        mosquitto__free(config->log_file);
        config->log_file = NULL;
    }
}

const char *mosquitto_client_username(const struct mosquitto *client)
{
    return client->id;
}

char *misc__trimblanks(char *str)
{
    char *endptr;

    if (str == NULL) {
        return NULL;
    }

    while (isspace((int)str[0])) {
        str++;
    }
    endptr = &str[strlen(str) - 1];
    while (endptr > str && isspace((int)endptr[0])) {
        endptr[0] = '\0';
        endptr--;
    }
    return str;
}

// Dummy definition of fork() to work around IDF warning: " warning: _fork is not implemented and will always fail"
// fork() is used in mosquitto.c to deamonize the broker, which we do not call.
pid_t fork(void)
{
    abort();
    return 0;
}

#ifdef CONFIG_IDF_TARGET_LINUX
extern void app_main(void);

int __wrap_main(int argc, char *argv[])
{
    app_main();
    return 0;
}
#endif
