/*
 * SPDX-FileCopyrightText: 2024 Roger Light <roger@atchoo.org>
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
 */
#include "mosquitto_internal.h"
#include "mosquitto_broker.h"
#include "memory_mosq.h"
#include "mqtt_protocol.h"
#include "send_mosq.h"
#include "util_mosq.h"
#include "utlist.h"
#include "lib_load.h"


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
    return MOSQ_ERR_SUCCESS;
}
