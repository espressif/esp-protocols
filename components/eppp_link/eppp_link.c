/*
 * SPDX-FileCopyrightText: 2019-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif_ppp.h"
#include "eppp_link.h"
#include "eppp_transport_eth.h"
#include "eppp_transport_spi.h"
#include "eppp_transport_uart.h"
#include "eppp_transport_sdio.h"
#include "eppp_transport.h"


#if CONFIG_EPPP_LINK_DEVICE_ETH
#define EPPP_NEEDS_TASK 0
#else
#define EPPP_NEEDS_TASK 1
#endif

static const int GOT_IPV4 = BIT0;
static const int CONNECTION_FAILED = BIT1;
#define CONNECT_BITS (GOT_IPV4|CONNECTION_FAILED)

static EventGroupHandle_t s_event_group = NULL;
static const char *TAG = "eppp_link";
static int s_retry_num = 0;
static int s_eppp_netif_count = 0; // used as a suffix for the netif key

void eppp_netif_deinit(esp_netif_t *netif)
{
    if (netif == NULL) {
        return;
    }
    esp_netif_destroy(netif);
    if (s_eppp_netif_count > 0) {
        s_eppp_netif_count--;
    }
}

#ifdef CONFIG_EPPP_LINK_USES_PPP
#define NETSTACK_CONFIG() ESP_NETIF_NETSTACK_DEFAULT_PPP
#else
#define NETSTACK_CONFIG() g_eppp_netif_config_tun
extern esp_netif_netstack_config_t *g_eppp_netif_config_tun;
#define ESP_NETIF_INHERENT_DEFAULT_SLIP() \
    {   \
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac) \
        ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
        .flags = ESP_NETIF_FLAG_EVENT_IP_MODIFIED,              \
        .get_ip_event = IP_EVENT_PPP_GOT_IP,    \
        .lost_ip_event = IP_EVENT_PPP_LOST_IP,   \
        .if_key = "EPPP_TUN",  \
        .if_desc = "eppp",    \
        .route_prio = 1,     \
        .bridge_info = NULL \
};
#endif // CONFIG_EPPP_LINK_USES_PPP

esp_netif_t *eppp_netif_init(eppp_type_t role, eppp_transport_handle_t h, eppp_config_t *eppp_config)
{
    if (s_eppp_netif_count > 9) {   // Limit to max 10 netifs, since we use "EPPPx" as the unique key (where x is 0-9)
        ESP_LOGE(TAG, "Cannot create more than 10 instances");
        return NULL;
    }

    h->role = role;
#ifdef CONFIG_EPPP_LINK_USES_PPP
    esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
#else
    esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_SLIP();
    esp_netif_ip_info_t slip_ip4 = {};
    slip_ip4.ip.addr = eppp_config->ppp.our_ip4_addr.addr;
    slip_ip4.gw.addr = eppp_config->ppp.their_ip4_addr.addr;
    slip_ip4.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    base_netif_cfg.ip_info = &slip_ip4;
#endif
    char if_key[] = "EPPP0"; // netif key needs to be unique
    if_key[sizeof(if_key) - 2 /* 2 = two chars before the terminator */ ] += s_eppp_netif_count++;
    base_netif_cfg.if_key = if_key;
    if (eppp_config->ppp.netif_description) {
        base_netif_cfg.if_desc = eppp_config->ppp.netif_description;
    } else {
        base_netif_cfg.if_desc = role == EPPP_CLIENT ? "pppos_client" : "pppos_server";
    }
    if (eppp_config->ppp.netif_prio) {
        base_netif_cfg.route_prio = eppp_config->ppp.netif_prio;
    }
    esp_netif_config_t netif_config = { .base = &base_netif_cfg,
                                        .driver = NULL,
                                        .stack = NETSTACK_CONFIG(),
                                      };

#ifdef CONFIG_EPPP_LINK_USES_PPP
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    esp_netif_t *netif = esp_netif_new(&netif_config);
    esp_netif_ppp_config_t netif_params;
    ESP_GOTO_ON_ERROR(esp_netif_ppp_get_params(netif, &netif_params), err, TAG, "Failed to get PPP params");
    netif_params.ppp_our_ip4_addr = eppp_config->ppp.our_ip4_addr;
    netif_params.ppp_their_ip4_addr = eppp_config->ppp.their_ip4_addr;
    netif_params.ppp_error_event_enabled = true;
    ESP_GOTO_ON_ERROR(esp_netif_ppp_set_params(netif, &netif_params), err, TAG, "Failed to set PPP params");
    return netif;
err:
    esp_netif_destroy(netif);
    return NULL;
#else
    return esp_netif_new(&netif_config);
#endif // CONFIG_EPPP_LINK_USES_PPP
}

esp_err_t eppp_netif_stop(esp_netif_t *netif, int stop_timeout_ms)
{
    esp_netif_action_disconnected(netif, 0, 0, 0);
    esp_netif_action_stop(netif, 0, 0, 0);
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    for (int wait = 0; wait < 100; wait++) {
        vTaskDelay(pdMS_TO_TICKS(stop_timeout_ms) / 100);
        if (h->netif_stop) {
            break;
        }
    }
    if (!h->netif_stop) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t eppp_netif_start(esp_netif_t *netif)
{
    esp_netif_action_start(netif, 0, 0, 0);
    esp_netif_action_connected(netif, 0, 0, 0);
#ifndef CONFIG_EPPP_LINK_USES_PPP
    // PPP provides address negotiation, if not PPP, we need to check connection manually
    return eppp_check_connection(netif);
#else
    return ESP_OK;
#endif
}

static int get_netif_num(esp_netif_t *netif)
{
    if (netif == NULL) {
        return -1;
    }
    const char *ifkey = esp_netif_get_ifkey(netif);
    if (strstr(ifkey, "EPPP") == NULL) {
        return -1; // not our netif
    }
    int netif_cnt = ifkey[4] - '0';
    if (netif_cnt < 0 || netif_cnt > 9) {
        ESP_LOGE(TAG, "Unexpected netif key %s", ifkey);
        return -1;
    }
    return  netif_cnt;
}

#ifdef CONFIG_EPPP_LINK_USES_PPP
static void on_ppp_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_netif_t **netif = data;
    if (base == NETIF_PPP_STATUS && event_id == NETIF_PPP_ERRORUSER) {
        ESP_LOGI(TAG, "Disconnected %d", get_netif_num(*netif));
        struct eppp_handle *h = esp_netif_get_io_driver(*netif);
        h->netif_stop = true;
    }
}
#endif // CONFIG_EPPP_LINK_USES_PPP

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    esp_netif_t *netif = event->esp_netif;
    int netif_cnt = get_netif_num(netif);
    if (netif_cnt < 0) {
        return;
    }
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s(%s)\" address: " IPSTR, esp_netif_get_desc(netif),
                 esp_netif_get_ifkey(netif), IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_event_group, GOT_IPV4 << (netif_cnt * 2));
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Disconnected");
        s_retry_num++;
        if (s_retry_num > CONFIG_EPPP_LINK_CONN_MAX_RETRY) {
            ESP_LOGE(TAG, "PPP Connection failed %d times, stop reconnecting.", s_retry_num);
            xEventGroupSetBits(s_event_group, CONNECTION_FAILED << (netif_cnt * 2));
        } else {
            ESP_LOGI(TAG, "PPP Connection failed %d times, try to reconnect.", s_retry_num);
            eppp_netif_start(netif);
        }
    }
}

#if EPPP_NEEDS_TASK
static void ppp_task(void *args)
{
    esp_netif_t *netif = args;
    while (eppp_perform(netif) != ESP_ERR_TIMEOUT) {}
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    h->exited = true;
    vTaskDelete(NULL);
}
#endif

static bool have_some_eppp_netif(esp_netif_t *netif, void *ctx)
{
    return get_netif_num(netif) > 0;
}

static void remove_handlers(void)
{
    esp_netif_t *netif = esp_netif_find_if(have_some_eppp_netif, NULL);
    if (netif == NULL) {
        // if EPPP netif in the system, we clean up the statics
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event);
#ifdef CONFIG_EPPP_LINK_USES_PPP
        esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_ppp_event);
#endif
    }
}

void eppp_deinit(esp_netif_t *netif)
{
    if (netif == NULL) {
        return;
    }
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    EPPP_TRANSPORT_DEINIT(h);
    eppp_netif_deinit(netif);
}

esp_netif_t *eppp_init(eppp_type_t role, eppp_config_t *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    esp_netif_t *netif = NULL;
    if (config == NULL || (role != EPPP_SERVER && role != EPPP_CLIENT)) {
        ESP_LOGE(TAG, "Invalid configuration or role");
        return NULL;
    }

    eppp_transport_handle_t h = EPPP_TRANSPORT_INIT(config);
    ESP_GOTO_ON_FALSE(h, ESP_ERR_NO_MEM, err, TAG, "Failed to init EPPP transport");
    ESP_GOTO_ON_FALSE(netif = eppp_netif_init(role, h, config), ESP_ERR_NO_MEM, err, TAG, "Failed to init EPPP netif");
    ESP_GOTO_ON_ERROR(esp_netif_attach(netif, h), err, TAG, "Failed to attach netif to EPPP transport");
    return netif;
err:
    if (h) {
        EPPP_TRANSPORT_DEINIT(h);
    }
    if (netif) {
        eppp_netif_deinit(netif);
    }
    return NULL;
}

esp_netif_t *eppp_open(eppp_type_t role, eppp_config_t *config, int connect_timeout_ms)
{
    if (config == NULL || (role != EPPP_SERVER && role != EPPP_CLIENT)) {
        ESP_LOGE(TAG, "Invalid configuration or role");
        return NULL;
    }
#if CONFIG_EPPP_LINK_DEVICE_UART
    if (config->transport != EPPP_TRANSPORT_UART) {
        ESP_LOGE(TAG, "Invalid transport: UART device must be enabled in Kconfig");
        return NULL;
    }
#endif
#if CONFIG_EPPP_LINK_DEVICE_SPI
    if (config->transport != EPPP_TRANSPORT_SPI) {
        ESP_LOGE(TAG, "Invalid transport: SPI device must be enabled in Kconfig");
        return NULL;
    }
#endif
#if CONFIG_EPPP_LINK_DEVICE_SDIO
    if (config->transport != EPPP_TRANSPORT_SDIO) {
        ESP_LOGE(TAG, "Invalid transport: SDIO device must be enabled in Kconfig");
        return NULL;
    }
#endif

    if (config->task.run_task == false) {
        ESP_LOGE(TAG, "task.run_task == false is invalid in this API. Please use eppp_init()");
        return NULL;
    }
    if (s_event_group == NULL) {
        s_event_group = xEventGroupCreate();
        if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register IP event handler");
            remove_handlers();
            return NULL;
        }
#ifdef CONFIG_EPPP_LINK_USES_PPP
        if (esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_ppp_event, NULL) !=  ESP_OK) {
            ESP_LOGE(TAG, "Failed to register PPP status handler");
            remove_handlers();
            return NULL;
        }
#endif // CONFIG_EPPP_LINK_USES_PPP
    }
    esp_netif_t *netif = eppp_init(role, config);
    if (!netif) {
        remove_handlers();
        return NULL;
    }

    eppp_netif_start(netif);
#if EPPP_NEEDS_TASK
    if (xTaskCreate(ppp_task, "ppp connect", config->task.stack_size, netif, config->task.priority, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create a ppp connection task");
        eppp_deinit(netif);
        return NULL;
    }
#endif
    int netif_cnt = get_netif_num(netif);
    if (netif_cnt < 0) {
        eppp_close(netif);
        return NULL;
    }
    ESP_LOGI(TAG, "Waiting for IP address %d", netif_cnt);
    EventBits_t bits = xEventGroupWaitBits(s_event_group, CONNECT_BITS << (netif_cnt * 2), pdFALSE, pdFALSE, pdMS_TO_TICKS(connect_timeout_ms));
    if (bits & (CONNECTION_FAILED << (netif_cnt * 2))) {
        ESP_LOGE(TAG, "Connection failed!");
        eppp_close(netif);
        return NULL;
    }
    ESP_LOGI(TAG, "Connected! %d", netif_cnt);
    return netif;
}

esp_netif_t *eppp_connect(eppp_config_t *config)
{
    return eppp_open(EPPP_CLIENT, config, portMAX_DELAY);
}

esp_netif_t *eppp_listen(eppp_config_t *config)
{
    return eppp_open(EPPP_SERVER, config, portMAX_DELAY);
}

void eppp_close(esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    if (eppp_netif_stop(netif, 60000) != ESP_OK) {
        ESP_LOGE(TAG, "Network didn't exit cleanly");
    }
    h->stop = true;
    for (int wait = 0; wait < 100; wait++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (h->exited) {
            break;
        }
    }
    if (!h->exited) {
        ESP_LOGE(TAG, "Cannot stop ppp_task");
    }
    eppp_deinit(netif);
    remove_handlers();
}

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
esp_err_t eppp_add_channels(esp_netif_t *netif, eppp_channel_fn_t *tx, const eppp_channel_fn_t rx, void* context)
{
    ESP_RETURN_ON_FALSE(netif != NULL && tx != NULL && rx != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    ESP_RETURN_ON_FALSE(h != NULL && h->channel_tx != NULL, ESP_ERR_INVALID_STATE, TAG, "Transport not initialized");
    *tx = h->channel_tx;
    h->channel_rx = rx;
    h->context = context;
    return ESP_OK;
}

void* eppp_get_context(esp_netif_t *netif)
{
    ESP_RETURN_ON_FALSE(netif != NULL, NULL, TAG, "Invalid netif");
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    ESP_RETURN_ON_FALSE(h != NULL, NULL, TAG, "EPPP Not initialized");
    return h->context;
}
#endif
