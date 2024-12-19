/*
 * lws-minimal-ws-client-ping
 *
 * Written in 2010-2020 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * This demonstrates keeping a ws connection validated by the lws validity
 * timer stuff without having to do anything in the code.  Use debug logging
 * -d1039 to see lws doing the pings / pongs in the background.
 */

#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#if defined(WIN32)
#define HAVE_STRUCT_TIMESPEC
#if defined(pid_t)
#undef pid_t
#endif
#endif
#include <pthread.h>
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_system.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "esp_task_wdt.h"

#include "nvs_flash.h"
#include "ping/ping_sock.h"
#include "driver/gpio.h"

static int message_delay = 500000; // microseconds

#define SSID " "
#define PASS " "

static struct lws_context *context;
static struct lws *client_wsi;
static int interrupted, port = 80, ssl_connection = LCCSCF_ALLOW_INSECURE;
static const char *server_address = "ws.ifelse.io", *pro = "lws-mirror-protocol";
static lws_sorted_usec_list_t sul;
static unsigned char msg[LWS_PRE+128];

static const lws_retry_bo_t retry = {
	.secs_since_valid_ping = 3,
	.secs_since_valid_hangup = 10,
};

static void
connect_cb(lws_sorted_usec_list_t *_sul)
{
	struct lws_client_connect_info i;

	lwsl_notice("%s: connecting\n", __func__);

	memset(&i, 0, sizeof(i));

	i.context = context;
	i.port = port;
	i.address = server_address;
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = ssl_connection;
	i.protocol = pro;
	i.alpn = "h2;http/1.1";
	i.local_protocol_name = "lws-ping-test";
	i.pwsi = &client_wsi;
	i.retry_and_idle_policy = &retry;

	if (!lws_client_connect_via_info(&i))
		lws_sul_schedule(context, 0, _sul, connect_cb, 5 * LWS_USEC_PER_SEC);
}
uint8_t u8Count = 0;

static int
callback_minimal_pingtest(struct lws *wsi, enum lws_callback_reasons reason,
			 void *user, void *in, size_t len)
{

    //lwsl_notice("callback called with reason %d\n", reason);
	switch (reason) {

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
			 in ? (char *)in : "(null)");
		lws_sul_schedule(context, 0, &sul, connect_cb, 5 * LWS_USEC_PER_SEC);
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		lws_callback_on_writable(wsi);
		lwsl_user("%s: established connection, wsi = %p\n",
				__func__, wsi);
		break;
	case LWS_CALLBACK_CLIENT_WRITEABLE:

		//lwsl_user("%s: writable\n", __func__);
        //lwsl_user("TX ----> %s\n", n);
		u8Count++;
		char dummy;
		if(u8Count == 1)
		{
			dummy = 'F';
		}
		else
		{
			dummy = 'A';
			u8Count = 0;
		}

		msg[LWS_PRE + 1] = dummy;
		lwsl_warn("TX ----> ");
		lwsl_hexdump_warn(&dummy, sizeof(dummy));

		(void)lws_write(wsi, msg + LWS_PRE + 1, 1, LWS_WRITE_TEXT);


		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
		lwsl_notice("RX <---- ");
		lwsl_hexdump_notice(in, len);

			/*
		 * Schedule the timer after minimum message delay plus the
		 * random number of centiseconds.
		 */
		lws_set_timer_usecs(wsi, message_delay);
		break;
	case LWS_CALLBACK_TIMER:
		// Let the main loop know we want to send another message to the
		// server
		lws_callback_on_writable(wsi);
		break;
		break;
	default:
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{
		"lws-ping-test",
		callback_minimal_pingtest,
		4096, 4096, 0, NULL, 0
	},
	LWS_PROTOCOL_LIST_TERM
};

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS}};
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
}
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 3000,
        .idle_core_mask = 0,
        .trigger_panic = false,
    };

int app_main(int argc, const char **argv)
{
	wifi_connection();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

	struct lws_context_creation_info info;
	int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE
			/* for LLL_ verbosity above NOTICE to be built into lws,
			 * lws must have been configured and built with
			 * -DCMAKE_BUILD_TYPE=DEBUG instead of =RELEASE */
			/* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
			/* | LLL_EXT */ /* | LLL_CLIENT */ /* | LLL_LATENCY */
			/* | LLL_DEBUG */;


	memset(msg, 'x', sizeof(msg));
	esp_task_wdt_reconfigure(&twdt_config);


	lws_set_log_level(logs, NULL);
	lwsl_user("LWS minimal ws client PING\n");

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = protocols;
#if defined(LWS_WITH_MBEDTLS) || defined(USE_WOLFSSL)
	/*
	 * OpenSSL uses the system trust store.  mbedTLS has to be told which
	 * CA to trust explicitly.
	 */
	//info.client_ssl_ca_filepath = "./libwebsockets.org.cer";
#endif


	info.fd_limit_per_thread = 1 + 1 + 1;

	context = lws_create_context(&info);
	if (!context) {
		lwsl_err("lws init failed\n");
		return 1;
	}

	lws_sul_schedule(context, 0, &sul, connect_cb, 100);

	while (n >= 0 && !interrupted)
		n = lws_service(context, 0);

	lws_context_destroy(context);
	lwsl_user("Completed\n");

	return 0;
}
