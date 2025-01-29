/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ESP libwebsockets server example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <libwebsockets.h>
#include <stdio.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_task_wdt.h"

#define RING_DEPTH 4096

static int callback_minimal_server_echo(struct lws *wsi, enum lws_callback_reasons reason,
                                        void *user, void *in, size_t len);
/* one of these created for each message */

struct msg {
    void *payload; /* is malloc'd */
    size_t len;
};

/* one of these is created for each client connecting to us */

struct per_session_data__minimal {
    struct per_session_data__minimal *pss_list;
    struct lws *wsi;
    int last; /* the last message number we sent */
};

/* one of these is created for each vhost our protocol is used with */

struct per_vhost_data__minimal {
    struct lws_context *context;
    struct lws_vhost *vhost;
    const struct lws_protocols *protocol;

    struct per_session_data__minimal *pss_list; /* linked-list of live pss*/

    struct msg amsg; /* the one pending message... */
    int current; /* the current message number we are caching */
};

static struct lws_protocols protocols[] = {
    {
        .name = "lws-minimal-server-echo",
        .callback = callback_minimal_server_echo,
        .per_session_data_size = sizeof(struct per_session_data__minimal),
        .rx_buffer_size = 128,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0
    },
    LWS_PROTOCOL_LIST_TERM
};

static int options;
static const char *TAG = "lws-server-echo", *iface = "";

/* pass pointers to shared vars to the protocol */
static const struct lws_protocol_vhost_options pvo_options = {
    NULL,
    NULL,
    "options",      /* pvo name */
    (void *) &options   /* pvo value */
};

static const struct lws_protocol_vhost_options pvo_interrupted = {
    &pvo_options,
    NULL,
    "interrupted",      /* pvo name */
    NULL    /* pvo value */
};

static const struct lws_protocol_vhost_options pvo = {
    NULL,               /* "next" pvo linked-list */
    &pvo_interrupted,       /* "child" pvo linked-list */
    "lws-minimal-server-echo",  /* protocol name we belong to on this vhost */
    ""              /* ignored */
};

int app_main(int argc, const char **argv)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Configure WDT. */
    esp_task_wdt_config_t esp_task_wdt_config = {
        .idle_core_mask = 0,
        .timeout_ms = portMAX_DELAY,
        .trigger_panic = false
    };
    esp_task_wdt_reconfigure(&esp_task_wdt_config);

    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    esp_task_wdt_add(handle);

    /* Create LWS Context - Server. */
    struct lws_context_creation_info info;
    struct lws_context *context;
    int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    lws_set_log_level(logs, NULL);
    ESP_LOGI(TAG, "LWS minimal ws server echo\n");

    memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
    info.port = CONFIG_WEBSOCKET_PORT;
    info.iface = iface;
    info.protocols = protocols;
    info.pvo = &pvo;
    info.pt_serv_buf_size = 64 * 1024;

#ifdef CONFIG_WS_OVER_TLS
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    /* Configuring server certificates for mutual authentification */
    extern const char cert_start[] asm("_binary_server_cert_pem_start");    // Server certificate
    extern const char cert_end[]   asm("_binary_server_cert_pem_end");
    extern const char key_start[] asm("_binary_server_key_pem_start");      // Server private key
    extern const char key_end[]   asm("_binary_server_key_pem_end");
    extern const char cacert_start[] asm("_binary_ca_cert_pem_start");      // CA certificate
    extern const char cacert_end[] asm("_binary_ca_cert_pem_end");

    info.server_ssl_cert_mem            = cert_start;
    info.server_ssl_cert_mem_len        = cert_end - cert_start - 1;
    info.server_ssl_private_key_mem     = key_start;
    info.server_ssl_private_key_mem_len = key_end - key_start - 1;
    info.server_ssl_ca_mem              = cacert_start;
    info.server_ssl_ca_mem_len          = cacert_end - cacert_start;
#endif

    context = lws_create_context(&info);
    if (!context) {
        ESP_LOGE(TAG, "lws init failed\n");
        return 1;
    }

    while (n >= 0) {
        n = lws_service(context, 0);
    }

    lws_context_destroy(context);

    return 1;
}

static void __minimal_destroy_message(void *_msg)
{
    struct msg *msg = _msg;

    free(msg->payload);
    msg->payload = NULL;
    msg->len = 0;
}

static int callback_minimal_server_echo(struct lws *wsi, enum lws_callback_reasons reason,
                                        void *user, void *in, size_t len)
{
    {
        struct per_session_data__minimal *pss =
            (struct per_session_data__minimal *)user;
        struct per_vhost_data__minimal *vhd =
            (struct per_vhost_data__minimal *)
            lws_protocol_vh_priv_get(lws_get_vhost(wsi),
                                     lws_get_protocol(wsi));
        int m;

        switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
                                              lws_get_protocol(wsi),
                                              sizeof(struct per_vhost_data__minimal));
            vhd->context = lws_get_context(wsi);
            vhd->protocol = lws_get_protocol(wsi);
            vhd->vhost = lws_get_vhost(wsi);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            /* add ourselves to the list of live pss held in the vhd */
            lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
            pss->wsi = wsi;
            pss->last = vhd->current;
            break;

        case LWS_CALLBACK_CLOSED:
            /* remove our closing pss from the list of live pss */
            lws_ll_fwd_remove(struct per_session_data__minimal, pss_list,
                              pss, vhd->pss_list);
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (!vhd->amsg.payload) {
                break;
            }

            if (pss->last == vhd->current) {
                break;
            }

            /* notice we allowed for LWS_PRE in the payload already */
            m = lws_write(wsi, ((unsigned char *)vhd->amsg.payload) +
                          LWS_PRE, vhd->amsg.len, LWS_WRITE_TEXT);
            if (m < (int)vhd->amsg.len) {
                lwsl_err("ERROR %d writing to ws\n", m);
                return -1;
            }

            pss->last = vhd->current;
            break;

        case LWS_CALLBACK_RECEIVE:
            if (vhd->amsg.payload) {
                __minimal_destroy_message(&vhd->amsg);
            }

            vhd->amsg.len = len;
            /* notice we over-allocate by LWS_PRE */
            vhd->amsg.payload = malloc(LWS_PRE + len);
            if (!vhd->amsg.payload) {
                lwsl_user("OOM: dropping\n");
                break;
            }

            memcpy((char *)vhd->amsg.payload + LWS_PRE, in, len);
            vhd->current++;

            /*
             * let everybody know we want to write something on them
             * as soon as they are ready
             */
            lws_start_foreach_llp(struct per_session_data__minimal **,
                                  ppss, vhd->pss_list) {
                lws_callback_on_writable((*ppss)->wsi);
            } lws_end_foreach_llp(ppss, pss_list);
            break;

        default:
            break;
        }

        return 0;
    }

}
