/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include "net_connect.h"
#include "net_connect_private.h"
#include "net_iface_priv.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"

static const char *TAG = "net_connect";

/* Enough for Wi-Fi, Ethernet, Thread, and PPP, with a little spare. */
#define NET_CONNECT_MAX_IFACES 8

#if CONFIG_NET_CONNECT_IPV6
/* Names for the IPv6 address types, used when printing addresses. */
const char *net_connect_ipv6_addr_types_to_str[6] = {
    "ESP_IP6_ADDR_IS_UNKNOWN",
    "ESP_IP6_ADDR_IS_GLOBAL",
    "ESP_IP6_ADDR_IS_LINK_LOCAL",
    "ESP_IP6_ADDR_IS_SITE_LOCAL",
    "ESP_IP6_ADDR_IS_UNIQUE_LOCAL",
    "ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6"
};
#endif

/**
 * True when the netif description starts with prefix.
 * Every netif created by this component uses a "net_connect_netif_..." description.
 */
bool net_connect_is_our_netif(const char *prefix, esp_netif_t *netif)
{
    if (prefix == NULL || *prefix == '\0') {
        return false;
    }
    const char *netif_desc = esp_netif_get_desc(netif);
    if (netif_desc == NULL) {
        return false;
    }
    return strncmp(prefix, netif_desc, strlen(prefix)) == 0;
}

/**
 * Match a netif whose description is exactly ctx.
 * The second argument is void * because that is what esp_netif_find_if() passes.
 */
static bool netif_desc_matches_with(esp_netif_t *netif, void *ctx)
{
    const char *desc = (const char *)ctx;
    const char *netif_desc = esp_netif_get_desc(netif);
    if (netif_desc == NULL) {
        return false;
    }
    return strcmp(desc, netif_desc) == 0;
}

esp_netif_t *net_get_netif_from_desc(const char *desc)
{
    if (desc == NULL || *desc == '\0') {
        return NULL;
    }
    return esp_netif_find_if(netif_desc_matches_with, (void *)desc);
}

/* Print the addresses of one netif. Runs on the TCP/IP task. */
static esp_err_t print_all_ips_tcpip(void *ctx)
{
    const char *prefix = ctx;
    esp_netif_t *netif = NULL;
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        if (net_connect_is_our_netif(prefix, netif)) {
            const char *netif_desc = esp_netif_get_desc(netif);
            ESP_LOGI(TAG, "Connected to %s", netif_desc != NULL ? netif_desc : "(null)");
#if CONFIG_NET_CONNECT_IPV4
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
                ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&ip.ip));
            }
#endif
#if CONFIG_NET_CONNECT_IPV6
            esp_ip6_addr_t ip6[MAX_IP6_ADDRS_PER_NETIF];
            int ip6_addrs = esp_netif_get_all_ip6(netif, ip6);
            for (int j = 0; j < ip6_addrs; ++j) {
                esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&(ip6[j]));
                ESP_LOGI(TAG, "- IPv6 address: " IPV6STR ", type: %s", IPV62STR(ip6[j]), net_connect_ipv6_addr_types_to_str[ipv6_type]);
            }
#endif
        }
    }
    return ESP_OK;
}

void net_connect_print_all_netif_ips(const char *prefix)
{
    /* Print from the TCP/IP task so the netif list cannot change under us. */
    esp_netif_tcpip_exec(print_all_ips_tcpip, (void *)prefix);
}

/* A short list of interface handles, copied out of the registry before we change it. */
typedef struct {
    net_iface_handle_t items[NET_CONNECT_MAX_IFACES];
    size_t count;
} net_iface_list_t;

/* Add one handle to the list. Returning true keeps the walk going. */
static bool collect_iface(net_iface_handle_t h, void *ctx)
{
    net_iface_list_t *list = (net_iface_list_t *)ctx;
    if (h != NULL && list->count < NET_CONNECT_MAX_IFACES) {
        list->items[list->count++] = h;
    }
    return true;
}

/* Stop and free every interface. The list is copied first, because deinit changes it. */
static void stop_every_iface(void)
{
    net_iface_list_t list = { 0 };
    net_iface_for_each(collect_iface, &list);
    for (size_t i = 0; i < list.count; i++) {
        net_iface_stop(list.items[i]);
        net_iface_deinit(list.items[i]);
    }
}

/* True after the restart handler is registered. */
static bool s_shutdown_registered = false;

/* Called on restart. Stops every interface this component still owns. */
static void net_disconnect_shutdown(void)
{
    s_shutdown_registered = false;
    stop_every_iface();
}

/* Register the restart handler once. Already registered is success. */
static esp_err_t register_shutdown_once(void)
{
    if (s_shutdown_registered) {
        return ESP_OK;
    }
    esp_err_t err = esp_register_shutdown_handler(net_disconnect_shutdown);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        s_shutdown_registered = true;
        return ESP_OK;
    }
    return err;
}

/* 0 in Kconfig means wait with no time limit. */
static uint32_t connect_timeout_ms(void)
{
    return CONFIG_NET_CONNECT_TIMEOUT_MS == 0 ? UINT32_MAX : (uint32_t)CONFIG_NET_CONNECT_TIMEOUT_MS;
}

/*
 * Start one interface, and connect it when the config says to.
 * A required interface that stays idle or fails makes net_connect() fail.
 * An optional one is logged and left as it is.
 */
static esp_err_t start_iface(net_iface_handle_t h, const char *name)
{
    net_iface_state_t state = net_iface_get_state(h);
    if (state == NET_IFACE_STATE_INIT || state == NET_IFACE_STATE_FAILED) {
        esp_err_t err = net_iface_start(h);
        state = net_iface_get_state(h);
        if (err != ESP_OK &&
                state != NET_IFACE_STATE_STARTED &&
                state != NET_IFACE_STATE_STARTING &&
                state != NET_IFACE_STATE_CONNECTING &&
                state != NET_IFACE_STATE_CONNECTED &&
                state != NET_IFACE_STATE_READY) {
            if (h->cfg.required) {
                ESP_LOGE(TAG, "%s failed to start: %s", name, esp_err_to_name(err));
                return ESP_FAIL;
            }
            ESP_LOGW(TAG, "%s did not start (%s). It is optional, so other interfaces stay up.",
                     name, esp_err_to_name(err));
            return ESP_OK;
        }
    }

    state = net_iface_get_state(h);
    /* start() already connects when auto-connect is on and the driver is up. */
    if (state == NET_IFACE_STATE_STARTED && h->cfg.auto_connect_on_start) {
        esp_err_t err = net_iface_connect(h);
        state = net_iface_get_state(h);
        if (err != ESP_OK && state != NET_IFACE_STATE_STARTED) {
            if (h->cfg.required) {
                ESP_LOGE(TAG, "%s failed to connect: %s", name, esp_err_to_name(err));
                return ESP_FAIL;
            }
            ESP_LOGW(TAG, "%s did not connect (%s). It is optional, so other interfaces stay up.",
                     name, esp_err_to_name(err));
            return ESP_OK;
        }
    }

    state = net_iface_get_state(h);
    /*
     * start() returns while the driver is still coming up. An interface that
     * was configured not to connect (for example Wi-Fi with no network name)
     * will sit in "starting" and then "started". Fail or skip it now, so a
     * later wait does not block for the full timeout.
     */
    if (!h->cfg.auto_connect_on_start &&
            (state == NET_IFACE_STATE_STARTING || state == NET_IFACE_STATE_STARTED)) {
        if (h->cfg.required) {
            ESP_LOGE(TAG, "%s is required, but it is not configured to connect", name);
            return ESP_FAIL;
        }
        ESP_LOGW(TAG, "%s is not connecting. It is optional, so other interfaces stay up.", name);
        return ESP_OK;
    }

    if (state == NET_IFACE_STATE_STARTED || state == NET_IFACE_STATE_FAILED ||
            state == NET_IFACE_STATE_INIT) {
        if (h->cfg.required) {
            ESP_LOGE(TAG, "%s is required, but it is not connecting", name);
            return ESP_FAIL;
        }
        ESP_LOGW(TAG, "%s is not connecting. It is optional, so other interfaces stay up.", name);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "%s started", name);
    return ESP_OK;
}

/*
 * Wait for the interfaces started by net_connect().
 * Required interfaces share one timeout and all have to become ready.
 * When none are required, wait until one optional interface that is actually
 * connecting becomes ready. An optional failure does not stop the others.
 */
static esp_err_t wait_for_required(void)
{
    net_iface_list_t list = { 0 };
    net_iface_for_each(collect_iface, &list);
    uint32_t timeout_ms = connect_timeout_ms();

    net_iface_handle_t req_handles[NET_CONNECT_MAX_IFACES];
    size_t req_count = 0;
    net_iface_handle_t opt_handles[NET_CONNECT_MAX_IFACES];
    size_t opt_count = 0;

    /* The registry list is newest-first. Walk backwards to keep start order. */
    for (size_t n = list.count; n > 0; n--) {
        net_iface_handle_t h = list.items[n - 1];
        if (h->cfg.required) {
            req_handles[req_count++] = h;
        } else if (h->cfg.auto_connect_on_start) {
            net_iface_state_t st = net_iface_get_state(h);
            if (st != NET_IFACE_STATE_INIT && st != NET_IFACE_STATE_UNINIT &&
                    st != NET_IFACE_STATE_STARTED) {
                opt_handles[opt_count++] = h;
            }
        }
    }

    if (req_count > 0) {
        esp_err_t err = net_iface_wait_all(req_handles, req_count, timeout_ms);
        if (err != ESP_OK) {
            for (size_t i = 0; i < req_count; i++) {
                net_iface_handle_t h = req_handles[i];
                if (net_iface_get_state(h) != NET_IFACE_STATE_READY) {
                    const char *name = (h->cfg.desc != NULL) ? h->cfg.desc : "interface";
                    ESP_LOGE(TAG, "%s did not become ready: %s", name, esp_err_to_name(err));
                }
            }
            return ESP_FAIL;
        }
        /* An optional interface may already have failed. Leave the working ones up. */
        for (size_t n = list.count; n > 0; n--) {
            net_iface_handle_t h = list.items[n - 1];
            if (!h->cfg.required && net_iface_get_state(h) == NET_IFACE_STATE_FAILED) {
                const char *name = (h->cfg.desc != NULL) ? h->cfg.desc : "interface";
                ESP_LOGW(TAG, "%s failed. It is optional, so other interfaces stay up.", name);
            }
        }
        return ESP_OK;
    }

    if (opt_count > 0) {
        esp_err_t err = net_iface_wait_any(opt_handles, opt_count, timeout_ms, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "No optional interface became ready: %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

/* Drop the restart handler, then stop every interface. */
static void cleanup_all(void)
{
    if (s_shutdown_registered) {
        esp_err_t err = esp_unregister_shutdown_handler(net_disconnect_shutdown);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Failed to unregister shutdown handler: %s", esp_err_to_name(err));
        }
        s_shutdown_registered = false;
    }
    stop_every_iface();
}

esp_err_t net_connect(void)
{
#if CONFIG_NET_CONNECT_ETHERNET
    net_iface_handle_t eth = net_iface_get_by_type(NET_IFACE_TYPE_ETH);
    if (eth == NULL) {
        esp_err_t err = net_eth_create(NULL, &eth);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create Ethernet interface: %s", esp_err_to_name(err));
            goto fail;
        }
    }
    if (start_iface(eth, "Ethernet") != ESP_OK) {
        goto fail;
    }
#endif

#if CONFIG_NET_CONNECT_WIFI
    /* Reuse a handle from net_configure_wifi_sta() when the caller made one. */
    net_iface_handle_t wifi = net_iface_get_by_type(NET_IFACE_TYPE_WIFI_STA);
    if (wifi == NULL) {
        esp_err_t err = net_wifi_sta_create(NULL, &wifi);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create Wi-Fi interface: %s", esp_err_to_name(err));
            goto fail;
        }
    }
#if CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
    {
        esp_err_t err = net_connect_wifi_apply_stdin(wifi);
        if (err != ESP_OK) {
            if (wifi->cfg.required) {
                ESP_LOGE(TAG, "Failed to read Wi-Fi credentials: %s", esp_err_to_name(err));
                goto fail;
            }
            ESP_LOGW(TAG, "Wi-Fi credentials were not read. Wi-Fi is optional, so other interfaces stay up.");
        }
    }
#endif
    if (start_iface(wifi, "Wi-Fi") != ESP_OK) {
        goto fail;
    }
#endif

#if CONFIG_NET_CONNECT_THREAD
    net_iface_handle_t thread = net_iface_get_by_type(NET_IFACE_TYPE_THREAD);
    if (thread == NULL) {
        esp_err_t err = net_thread_create(NULL, &thread);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create Thread interface: %s", esp_err_to_name(err));
            goto fail;
        }
    }
    if (start_iface(thread, "Thread") != ESP_OK) {
        goto fail;
    }
#endif

#if CONFIG_NET_CONNECT_PPP
    net_iface_handle_t ppp = net_iface_get_by_type(NET_IFACE_TYPE_PPP);
    if (ppp == NULL) {
        esp_err_t err = net_ppp_create(NULL, &ppp);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create PPP interface: %s", esp_err_to_name(err));
            goto fail;
        }
    }
    if (start_iface(ppp, "PPP") != ESP_OK) {
        goto fail;
    }
#endif

    if (register_shutdown_once() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register shutdown handler");
        goto fail;
    }
    if (wait_for_required() != ESP_OK) {
        goto fail;
    }

#if CONFIG_NET_CONNECT_ETHERNET
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_ETH);
#endif
#if CONFIG_NET_CONNECT_WIFI
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_STA);
#endif
#if CONFIG_NET_CONNECT_THREAD
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_THREAD);
#endif
#if CONFIG_NET_CONNECT_PPP
    net_connect_print_all_netif_ips(NET_CONNECT_NETIF_DESC_PPP);
#endif
    return ESP_OK;

fail:
    cleanup_all();
    return ESP_FAIL;
}

esp_err_t net_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting network interfaces...");
    cleanup_all();
    ESP_LOGI(TAG, "All network interfaces disconnected");
    return ESP_OK;
}
