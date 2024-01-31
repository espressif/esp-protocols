/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "console_mqtt.h"
#include "mqtt_client.h"

static const char *TAG = "console_mqtt";

#define CONNECT_HELP_MSG "mqtt -C -h <host uri> -u <username> -P <password>\n"
#define PUBLISH_HELP_MSG "Usage: mqtt -P -t <topic> -d <data>\n"
#define SUBSCRIBE_HELP_MSG "Usage: mqtt -S -t <topic>\n"
#define UNSUBSCRIBE_HELP_MSG "Usage: mqtt -U\n"
#define DISCONNECT_HELP_MSG "Usage: mqtt -D\n"

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "console_cmd_mqtt",
    .plugin_regd_fn = &console_cmd_mqtt_register
};

static struct {
    struct arg_lit *connect;
    struct arg_str *uri;
    struct arg_str *username;
    struct arg_str *password;
    struct arg_lit *disconnect;

    struct arg_end *end;
} mqtt_args;

static struct {
    struct arg_str *topic;
    struct arg_lit *unsubscribe;

    struct arg_end *end;
} mqtt_sub_args;

static struct {
    struct arg_str *topic;
    struct arg_str *message;

    struct arg_end *end;
} mqtt_pub_args;

static esp_mqtt_client_handle_t client_handle = NULL;


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


static esp_mqtt_client_config_t
get_mqtt_config_basic(const char *uri_i)
{
    char *uri;
    asprintf(&uri, "%s", uri_i);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
    };

    return mqtt_cfg;
}


static esp_mqtt_client_config_t
get_mqtt_config_username_password(const char *uri_i, const char *username_i, const char *password_i)
{
    char *uri, *username, *password;

    asprintf(&uri, "%s", uri_i);
    asprintf(&username, "%s", username_i);
    asprintf(&password, "%s", password_i);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.username = username,
        .credentials.authentication.password = password
    };

    return mqtt_cfg;
}

static void mqtt_config_free(esp_mqtt_client_config_t mqtt_cfg)
{
    if (mqtt_cfg.broker.address.uri) {
        free((char *)mqtt_cfg.broker.address.uri);
    }

    if (mqtt_cfg.credentials.username) {
        free((char *)mqtt_cfg.credentials.username);
    }

    if (mqtt_cfg.credentials.authentication.password) {
        free((char *)mqtt_cfg.credentials.authentication.password);
    }
}

static int do_mqtt_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&mqtt_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mqtt_args.end, argv[0]);
        return 1;
    }

    if (mqtt_args.connect->count > 0) {

        esp_mqtt_client_config_t mqtt_cfg;

        if (client_handle != NULL) {
            ESP_LOGI(TAG, "mqtt client already connected");
            return 0;
        }

        if ((mqtt_args.username->count > 0) && (mqtt_args.password->count > 0)) {
            if (mqtt_args.uri->count > 0) {
                mqtt_cfg = get_mqtt_config_username_password(mqtt_args.uri->sval[0],
                           mqtt_args.username->sval[0],
                           mqtt_args.password->sval[0]);
            } else {
                mqtt_cfg = get_mqtt_config_username_password(CONFIG_MQTT_BROKER_URL,
                           mqtt_args.username->sval[0],
                           mqtt_args.password->sval[0]);
            }
        } else {
            if (mqtt_args.uri->count > 0) {
                mqtt_cfg = get_mqtt_config_basic(mqtt_args.uri->sval[0]);
            } else {
                mqtt_cfg = get_mqtt_config_basic(CONFIG_MQTT_BROKER_URL);
            }
        }

        ESP_LOGI(TAG, "broker: %s", mqtt_cfg.broker.address.uri);

        client_handle = esp_mqtt_client_init(&mqtt_cfg);
        if (client_handle == NULL) {
            ESP_LOGE(TAG, "ERROR: Client init");
            return 1;
        }

        /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
        esp_mqtt_client_register_event(client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client_handle);

        mqtt_config_free(mqtt_cfg);

    } else if (mqtt_args.disconnect->count > 0) {
        ESP_LOGD(TAG, "Disconnect command received:");

        if (client_handle == NULL) {
            ESP_LOGE(TAG, "mqtt client not connected");
            return 0;
        }

        if (esp_mqtt_client_stop(client_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop mqtt client task");
            return 1;
        }

        client_handle = NULL;
        ESP_LOGI(TAG, "mqtt client disconnected");
    }

    return 0;
}


static int do_mqtt_sub_cmd(int argc, char **argv)
{
    int msg_id;
    int nerrors = arg_parse(argc, argv, (void **)&mqtt_sub_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mqtt_sub_args.end, argv[0]);
        return 1;
    }

    if (client_handle == NULL) {
        ESP_LOGE(TAG, "mqtt client not connected");
        return 0;
    }

    if (mqtt_sub_args.unsubscribe->count > 0) {

        if (mqtt_sub_args.topic->count <= 0) {
            ESP_LOGE(TAG, UNSUBSCRIBE_HELP_MSG);
        }
        char *topic = (char *)mqtt_sub_args.topic->sval[0];

        msg_id = esp_mqtt_client_unsubscribe(client_handle, mqtt_sub_args.topic->sval[0]);
        ESP_LOGI(TAG, "Unsubscribe successful, msg_id=%d, topic=%s", msg_id, topic);
    } else {

        if (mqtt_sub_args.topic->count <= 0) {
            ESP_LOGE(TAG, SUBSCRIBE_HELP_MSG);
        }
        char *topic = (char *)mqtt_sub_args.topic->sval[0];

        msg_id = esp_mqtt_client_subscribe(client_handle, topic, 0);
        ESP_LOGI(TAG, "Uubscribe successful, msg_id=%d, topic=%s", msg_id, topic);
    }

    return 0;
}


static int do_mqtt_pub_cmd(int argc, char **argv)
{
    int msg_id;
    int nerrors = arg_parse(argc, argv, (void **)&mqtt_pub_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mqtt_pub_args.end, argv[0]);
        return 1;
    }

    if (client_handle == NULL) {
        ESP_LOGE(TAG, "mqtt client not connected");
        return 0;
    }

    if ((mqtt_pub_args.topic->count <= 0) || (mqtt_pub_args.message->count <= 0)) {
        ESP_LOGE(TAG, PUBLISH_HELP_MSG);
    }

    msg_id = esp_mqtt_client_publish(client_handle,
                                     mqtt_pub_args.topic->sval[0],
                                     mqtt_pub_args.message->sval[0],
                                     0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "mqtt client not connected");
        return 0;
    }
    ESP_LOGI(TAG, "Publish successful, msg_id=%d, topic=%s, data=%s",
             msg_id, mqtt_pub_args.topic->sval[0], mqtt_pub_args.message->sval[0]);

    return 0;
}

/**
 * @brief Registers the mqtt commands.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_mqtt_register(void)
{
    esp_err_t ret = ESP_OK;

    /* Register mqtt */
    mqtt_args.connect = arg_lit0("C", "connect", "Connect to a broker");
    mqtt_args.uri = arg_str0("h", "host", "<host>", "Specify the host uri to connect to");
    mqtt_args.username = arg_str0("u", "username", "<username>", "Provide a username to be used for authenticating with the broker");
    mqtt_args.password = arg_str0("P", "password", "<password>", "Provide a password to be used for authenticating with the broker");
    mqtt_args.disconnect = arg_lit0("D", "disconnect", "Disconnect from the broker");
    mqtt_args.end = arg_end(1);

    const esp_console_cmd_t mqtt_cmd = {
        .command = "mqtt",
        .help = "mqtt command",
        .hint = NULL,
        .func = &do_mqtt_cmd,
        .argtable = &mqtt_args
    };

    ret = esp_console_cmd_register(&mqtt_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to register mqtt");
        return ret;
    }

    /* Register mqtt_pub */
    mqtt_pub_args.topic = arg_str0("t", "topic", "<topic>", "Topic to Subscribe/Publish");
    mqtt_pub_args.message = arg_str0("m", "message", "<message>", "Message to Publish");
    mqtt_pub_args.end = arg_end(1);

    const esp_console_cmd_t mqtt_pub_cmd = {
        .command = "mqtt_pub",
        .help = "mqtt publish command",
        .hint = NULL,
        .func = &do_mqtt_pub_cmd,
        .argtable = &mqtt_pub_args
    };

    ret = esp_console_cmd_register(&mqtt_pub_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to register mqtt_pub");
        return ret;
    }

    /* Register mqtt_sub */
    mqtt_sub_args.topic = arg_str0("t", "topic", "<topic>", "Topic to Subscribe/Publish");
    mqtt_sub_args.unsubscribe = arg_lit0("U", "unsubscribe", "Unsubscribe from a topic");
    mqtt_sub_args.end = arg_end(1);

    const esp_console_cmd_t mqtt_sub_cmd = {
        .command = "mqtt_sub",
        .help = "mqtt subscribe command",
        .hint = NULL,
        .func = &do_mqtt_sub_cmd,
        .argtable = &mqtt_sub_args
    };

    ret = esp_console_cmd_register(&mqtt_sub_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to register mqtt_sub");
    }

    return ret;
}
