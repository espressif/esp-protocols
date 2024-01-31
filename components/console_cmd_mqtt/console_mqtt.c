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
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include "esp_crt_bundle.h"
#endif

static const char *TAG = "console_mqtt";

#define CONNECT_HELP_MSG "mqtt -C -h <host uri> -u <username> -P <password> --cert --key --cafile\n"
#define PUBLISH_HELP_MSG "Usage: mqtt -P -t <topic> -d <data>\n"
#define SUBSCRIBE_HELP_MSG "Usage: mqtt -S -t <topic>\n"
#define UNSUBSCRIBE_HELP_MSG "Usage: mqtt -U\n"
#define DISCONNECT_HELP_MSG "Usage: mqtt -D\n"

#if CONFIG_MQTT_CMD_AUTO_REGISTRATION
/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "console_cmd_mqtt",
    .plugin_regd_fn = &console_cmd_mqtt_register
};
#endif

static struct {
    struct arg_lit *connect;
    struct arg_str *uri;
    struct arg_lit *status;
    struct arg_str *username;
    struct arg_str *password;
    struct arg_lit *cert;
    struct arg_lit *key;
    struct arg_lit *cafile;
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    struct arg_lit *use_internal_bundle;
#endif
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

typedef enum {
    MQTT_STATE_INIT = 0,
    MQTT_STATE_DISCONNECTED,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR,
    MQTT_STATE_STOPPED,
} mqtt_client_state_t;

mqtt_client_state_t client_status = MQTT_STATE_INIT;

static esp_mqtt_client_handle_t client_handle = NULL;

static const uint8_t *s_own_cert_pem_start = NULL;
static const uint8_t *s_own_cert_pem_end = NULL;
static const uint8_t *s_own_key_pem_start = NULL;
static const uint8_t *s_own_key_pem_end = NULL;
static const uint8_t *s_ca_cert_pem_start = NULL;
static const uint8_t *s_ca_cert_pem_end = NULL;

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
        client_status = MQTT_STATE_CONNECTED;
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        client_status = MQTT_STATE_DISCONNECTED;
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
        client_status = MQTT_STATE_ERROR;
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


static const char *mqtt_state_to_string(mqtt_client_state_t state)
{
    switch (state) {
    case MQTT_STATE_INIT:
        return "Initializing";
    case MQTT_STATE_DISCONNECTED:
        return "Disconnected";
    case MQTT_STATE_CONNECTED:
        return "Connected";
    case MQTT_STATE_ERROR:
        return "Error";
    case MQTT_STATE_STOPPED:
        return "Disconnected and Stopped";
    default:
        return "Unknown State";
    }
}


static int do_mqtt_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&mqtt_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mqtt_args.end, argv[0]);
        return 1;
    }

    if (mqtt_args.status->count > 0) {
        ESP_LOGI(TAG, "MQTT Client Status: %s\n", mqtt_state_to_string(client_status));
        return 0;
    }

    if (mqtt_args.connect->count > 0) {

        if (client_handle != NULL) {
            ESP_LOGW(TAG, "mqtt client already connected");
            ESP_LOGI(TAG, "Try: %s", DISCONNECT_HELP_MSG);
            return 0;
        }

        char *uri = CONFIG_MQTT_BROKER_URL;
        if (mqtt_args.uri->count > 0) {
            uri = (char *)mqtt_args.uri->sval[0];
        }

        esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = uri,
        };

        if ((mqtt_args.username->count > 0) && (mqtt_args.password->count > 0)) {
            mqtt_cfg.credentials.username = mqtt_args.username->sval[0];
            mqtt_cfg.credentials.authentication.password = mqtt_args.password->sval[0];
        }

        ESP_LOGI(TAG, "broker: %s", mqtt_cfg.broker.address.uri);

#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        /* Ensure --use_internal_bundle and --cafile are mutually exclusive */
        if ((mqtt_args.use_internal_bundle->count > 0) && (mqtt_args.cafile->count > 0)) {
            ESP_LOGE(TAG, "Error: Options can't be used together. Use either --use-internal-bundle or --cafile.  \n");
            return 1;
        }

        if (mqtt_args.use_internal_bundle->count > 0) {
            mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        }
#endif

        if (mqtt_args.cafile->count > 0) {
            if (s_ca_cert_pem_start && s_ca_cert_pem_end) {
                mqtt_cfg.broker.verification.certificate = (const char *)s_ca_cert_pem_start;
            } else {
                ESP_LOGW(TAG, "cafile not provided");
            }
        }

        if (mqtt_args.cert->count > 0) {
            if (s_own_cert_pem_start && s_own_cert_pem_end) {
                mqtt_cfg.credentials.authentication.certificate = (const char *)s_own_cert_pem_start;
            } else {
                ESP_LOGW(TAG, "cert not provided");
            }

            if (mqtt_args.key->count > 0) {
                if (s_own_key_pem_start && s_own_key_pem_end) {
                    mqtt_cfg.credentials.authentication.key = (const char *)s_own_key_pem_start;
                } else {
                    ESP_LOGW(TAG, "key not provided");
                }
            } else {
                mqtt_cfg.credentials.authentication.key = NULL;
            }
        }

        client_handle = esp_mqtt_client_init(&mqtt_cfg);
        if (client_handle == NULL) {
            ESP_LOGE(TAG, "ERROR: Client init");
            ESP_LOGI(TAG, "Try: %s", DISCONNECT_HELP_MSG);
            ESP_LOGE(TAG, CONNECT_HELP_MSG);
            return 1;
        }

        /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
        esp_mqtt_client_register_event(client_handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client_handle);

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
        client_status = MQTT_STATE_STOPPED;
        ESP_LOGI(TAG, "mqtt client disconnected and stopped");
    }

    return 0;
}


esp_err_t set_mqtt_client_cert(const uint8_t *client_cert_pem_start_i, const uint8_t *client_cert_pem_end_i)
{
    if (!client_cert_pem_start_i || !client_cert_pem_end_i ||
            (client_cert_pem_start_i > client_cert_pem_end_i)) {
        ESP_LOGE(TAG, "Invalid mqtt Client certs(%d)\n", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }

    s_own_cert_pem_start = client_cert_pem_start_i;
    s_own_cert_pem_end = client_cert_pem_end_i;

    return ESP_OK;
}


esp_err_t set_mqtt_client_key(const uint8_t *client_key_pem_start_i, const uint8_t *client_key_pem_end_i)
{
    if (client_key_pem_start_i && client_key_pem_end_i &&
            (client_key_pem_start_i >= client_key_pem_end_i)) {
        ESP_LOGE(TAG, "Invalid mqtt Client key(%d)\n", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }

    s_own_key_pem_start = client_key_pem_start_i;
    s_own_key_pem_end = client_key_pem_end_i;

    return ESP_OK;
}


esp_err_t set_mqtt_broker_certs(const uint8_t *ca_cert_pem_start_i, const uint8_t *ca_cert_pem_end_i)
{
    if (!ca_cert_pem_start_i || !ca_cert_pem_end_i ||
            (ca_cert_pem_start_i > ca_cert_pem_end_i)) {
        ESP_LOGE(TAG, "Invalid mqtt ca cert(%d)\n", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }

    s_ca_cert_pem_start = ca_cert_pem_start_i;
    s_ca_cert_pem_end = ca_cert_pem_end_i;

    return ESP_OK;
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
            return 0;
        }
        char *topic = (char *)mqtt_sub_args.topic->sval[0];

        msg_id = esp_mqtt_client_unsubscribe(client_handle, mqtt_sub_args.topic->sval[0]);
        ESP_LOGI(TAG, "Unsubscribe successful, msg_id=%d, topic=%s", msg_id, topic);

    } else {
        if (mqtt_sub_args.topic->count <= 0) {
            ESP_LOGE(TAG, SUBSCRIBE_HELP_MSG);
            return 0;
        }
        char *topic = (char *)mqtt_sub_args.topic->sval[0];

        msg_id = esp_mqtt_client_subscribe(client_handle, topic, 0);
        ESP_LOGI(TAG, "Subscribe successful, msg_id=%d, topic=%s", msg_id, topic);

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
    mqtt_args.connect = arg_lit0("C", "connect", "Connect to a broker (flag, no argument)");
    mqtt_args.uri = arg_str0("h", "host", "<host>", "Specify the host uri to connect to");
    mqtt_args.status = arg_lit0("s", "status", "Displays the status of the mqtt client (flag, no argument)");
    mqtt_args.username = arg_str0("u", "username", "<username>", "Provide a username to be used for authenticating with the broker");
    mqtt_args.password = arg_str0("P", "password", "<password>", "Provide a password to be used for authenticating with the broker");
    mqtt_args.cert = arg_lit0(NULL, "cert", "Define the PEM encoded certificate for this client, if required by the broker (flag, no argument)");
    mqtt_args.key = arg_lit0(NULL, "key", "Define the PEM encoded private key for this client, if required by the broker (flag, no argument)");
    mqtt_args.cafile = arg_lit0(NULL, "cafile", "Define the PEM encoded CA certificates that are trusted (flag, no argument)");
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    mqtt_args.use_internal_bundle = arg_lit0(NULL, "use-internal-bundle", "Use the internal certificate bundle for TLS (flag, no argument)");
#endif
    mqtt_args.disconnect = arg_lit0("D", "disconnect", "Disconnect from the broker (flag, no argument)");
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
