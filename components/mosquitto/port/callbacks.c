/*
 * SPDX-FileCopyrightText: 2024 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * SPDX-FileContributor: 2024-2025 Espressif Systems (Shanghai) CO LTD
 */
#include <string.h>
#include "mosquitto_internal.h"
#include "mosquitto_broker.h"
#include "memory_mosq.h"
#include "mqtt_protocol.h"
#include "send_mosq.h"
#include "util_mosq.h"
#include "utlist.h"
#include "lib_load.h"
#include "mosq_broker.h"

mosq_message_cb_t g_mosq_message_callback = NULL;
mosq_connect_cb_t g_mosq_connect_callback = NULL;

int mosquitto_callback_register(
    mosquitto_plugin_id_t *identifier,
    int event,
    MOSQ_FUNC_generic_callback cb_func,
    const void *event_data,
    void *userdata)
{
    return MOSQ_ERR_INVAL;
}

int mosquitto_callback_unregister(
    mosquitto_plugin_id_t *identifier,
    int event,
    MOSQ_FUNC_generic_callback cb_func,
    const void *event_data)
{
    return MOSQ_ERR_INVAL;
}

void plugin__handle_tick(void)
{
}

void plugin__handle_disconnect(struct mosquitto *context, int reason)
{
}

int plugin__handle_message(struct mosquitto *context, struct mosquitto_msg_store *stored)
{
    if (g_mosq_message_callback) {
        g_mosq_message_callback(context->id, stored->topic, stored->payload, stored->payloadlen, stored->qos, stored->retain);
    }
    return MOSQ_ERR_SUCCESS;
}

int __real_mosquitto_unpwd_check(struct mosquitto *context);

/* Wrapper function to intercept mosquitto_unpwd_check calls via linker wrapping */
int __wrap_mosquitto_unpwd_check(struct mosquitto *context)
{
    int rc;
    int password_len = 0;

    /* Call user's connect callback if set */
    if (g_mosq_connect_callback) {
        /* Extract password length if password is present.
         * Note: MQTT passwords are binary data, but mosquitto stores them as null-terminated strings.
         * If password contains null bytes, strlen() will not return the full length.
         * This matches how mosquitto itself handles passwords in some security functions. */
        if (context->password) {
            password_len = (int)strlen(context->password);
        }

        /* Call user callback */
        rc = g_mosq_connect_callback(
                 context->id ? context->id : "",
                 context->username ? context->username : NULL,
                 context->password ? context->password : NULL,
                 password_len
             );

        /* If callback rejects (returns non-zero), return AUTH error immediately */
        if (rc != 0) {
            return MOSQ_ERR_AUTH;
        }
    }

    /* Call the original function */
    return __real_mosquitto_unpwd_check(context);
}
