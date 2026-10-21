/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Ethernet driver.
 *
 * start() creates the netif and attaches the MAC. It does not start the PHY.
 * connect() calls esp_eth_start() and returns. Link-up and link-down events
 * are reported to net_iface.c. IP addresses are handled there as well.
 */

#include <string.h>
#include "net_connect.h"
#include "net_iface_priv.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "ethernet_init.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "net_connect_ethernet";

/* Board init returns these. Only the first port is used. */
static esp_eth_handle_t *s_eth_handles = NULL;
static uint8_t s_eth_count = 0;
static esp_eth_netif_glue_handle_t s_eth_glue = NULL;
static esp_netif_t *s_eth_netif = NULL;

/* Link up means the cable is connected. Link down means the address is gone. */
static void on_eth_event(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data)
{
    (void)event_base;
    (void)event_data;
    net_iface_handle_t self = (net_iface_handle_t)arg;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        net_iface_notify_connected(self);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        net_iface_notify_lost_ip(self);
        break;
    default:
        break;
    }
}

/* Release every Ethernet object this driver created. Safe to call more than once. */
static void eth_release_resources(void)
{
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event);

    if (s_eth_handles != NULL) {
        esp_err_t err = esp_eth_stop(s_eth_handles[0]);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "esp_eth_stop returned %s", esp_err_to_name(err));
        }
    }
    if (s_eth_glue != NULL) {
        (void)esp_eth_del_netif_glue(s_eth_glue);
        s_eth_glue = NULL;
    }
    if (s_eth_netif != NULL) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }
    if (s_eth_handles != NULL) {
        ethernet_deinit_all(s_eth_handles);
        s_eth_handles = NULL;
        s_eth_count = 0;
    }
}

/* Install the Ethernet driver and create the netif. The PHY is started later. */
static esp_err_t eth_ops_start(net_iface_handle_t self)
{
    if (s_eth_netif != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ethernet_init_all(&s_eth_handles, &s_eth_count);
    if (err != ESP_OK) {
        s_eth_handles = NULL;
        s_eth_count = 0;
        return err;
    }
    if (s_eth_handles == NULL || s_eth_count == 0) {
        return ESP_FAIL;
    }

    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.if_desc = self->cfg.desc != NULL ? self->cfg.desc : NET_CONNECT_NETIF_DESC_ETH;
    base.route_prio = self->cfg.route_prio > 0 ? self->cfg.route_prio : NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO;
    esp_netif_config_t netif_config = {
        .base = &base,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    s_eth_netif = esp_netif_new(&netif_config);
    if (s_eth_netif == NULL) {
        ethernet_deinit_all(s_eth_handles);
        s_eth_handles = NULL;
        s_eth_count = 0;
        return ESP_ERR_NO_MEM;
    }

    s_eth_glue = esp_eth_new_netif_glue(s_eth_handles[0]);
    if (s_eth_glue == NULL) {
        eth_release_resources();
        return ESP_ERR_NO_MEM;
    }

    err = esp_netif_attach(s_eth_netif, s_eth_glue);
    if (err != ESP_OK) {
        eth_release_resources();
        return err;
    }

    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event, self);
    if (err != ESP_OK) {
        eth_release_resources();
        return err;
    }

    net_iface_notify_started(self, s_eth_netif);
    return ESP_OK;
}

/* Start the PHY. The link-up event arrives later and is not waited for here. */
static esp_err_t eth_ops_connect(net_iface_handle_t self)
{
    (void)self;
    if (s_eth_handles == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_eth_start(s_eth_handles[0]);
}

/* Stop the PHY. Already stopped is fine. */
static esp_err_t eth_ops_disconnect(net_iface_handle_t self)
{
    (void)self;
    if (s_eth_handles == NULL) {
        return ESP_OK;
    }
    esp_err_t err = esp_eth_stop(s_eth_handles[0]);
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
}

/* Unregister events, stop the MAC, and free the netif. */
static esp_err_t eth_ops_stop(net_iface_handle_t self)
{
    (void)self;
    eth_release_resources();
    return ESP_OK;
}

/* Driver callbacks used by net_iface_init() and net_eth_create(). */
const net_iface_ops_t g_net_iface_eth_ops = {
    .name = "eth",
    .init = NULL,
    .deinit = NULL,
    .start = eth_ops_start,
    .stop = eth_ops_stop,
    .connect = eth_ops_connect,
    .disconnect = eth_ops_disconnect,
};

esp_err_t net_eth_create(const net_eth_params_t *params, net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    net_iface_config_t cfg = { 0 };
    cfg.type = NET_IFACE_TYPE_ETH;
    cfg.desc = NET_CONNECT_NETIF_DESC_ETH;
    cfg.route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO;
#if CONFIG_NET_CONNECT_ETHERNET_REQUIRED
    cfg.required = true;
#else
    cfg.required = false;
#endif
    cfg.auto_connect_on_start = true;
    cfg.ip.use_dhcp = true;
#if CONFIG_NET_CONNECT_IPV6
    cfg.ip.enable_ipv6 = true;
#else
    cfg.ip.enable_ipv6 = false;
#endif

    if (params != NULL) {
        if (params->route_prio >= 0) {
            cfg.route_prio = params->route_prio;
        }
        cfg.required = params->required;
        cfg.auto_connect_on_start = params->auto_connect_on_start;
        if (params->ip != NULL) {
            cfg.ip = *params->ip;
        }
    }

    return net_iface_init_with_ops(&cfg, &g_net_iface_eth_ops, out_handle);
}
