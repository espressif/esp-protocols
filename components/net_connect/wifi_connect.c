/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Wi-Fi station driver.
 *
 * start() turns the radio on and returns.
 * connect() tells the radio to join a network and returns. It does not wait
 * for an IP address. net_iface.c watches IP events and marks the interface ready.
 * The legacy net_connect_wifi() wrapper still waits, so older callers behave
 * the same as before.
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "net_connect.h"
#include "net_connect_private.h"
#include "net_connect_wifi_config.h"
#include "net_iface_priv.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#if CONFIG_NET_CONNECT_WIFI

static const char *TAG = "net_connect_wifi";

/* The one STA netif this driver created. NULL when the radio is off. */
static esp_netif_t *s_wifi_netif = NULL;

/* Handle remembered by net_configure_wifi_sta(). Cleared when that handle is freed. */
static net_iface_handle_t s_wifi_sta_handle = NULL;

/*
 * Set while this driver is dropping the link on purpose (disconnect or stop).
 * Disconnect events during that time must not start another join attempt.
 */
static bool s_wifi_ignore_disconnect = false;

/* Copy src into dst. The result is always terminated. NULL src becomes an empty string. */
static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/* Fill a station config with the values from Kconfig. */
static void populate_config_from_kconfig(net_wifi_sta_config_t *config)
{
    memset(config, 0, sizeof(*config));

#if !CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
    copy_text(config->ssid, sizeof(config->ssid), CONFIG_NET_CONNECT_WIFI_SSID);
    copy_text(config->password, sizeof(config->password), CONFIG_NET_CONNECT_WIFI_PASSWORD);
#endif

    config->scan_method = NET_CONNECT_WIFI_SCAN_METHOD;
    config->sort_method = NET_CONNECT_WIFI_CONNECT_AP_SORT_METHOD;
    config->threshold_rssi = CONFIG_NET_CONNECT_WIFI_SCAN_RSSI_THRESHOLD;
    config->auth_mode_threshold = NET_CONNECT_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    config->max_retry = CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY;
    config->ip.use_dhcp = true;
    /* An unset Kconfig bool is not defined, so this stays false. */
#if CONFIG_NET_CONNECT_IPV6
    config->ip.enable_ipv6 = true;
#else
    config->ip.enable_ipv6 = false;
#endif
}

/* Copy our station settings into the struct that esp_wifi_set_config() expects. */
static void convert_to_wifi_config(const net_wifi_sta_config_t *net_config, wifi_config_t *wifi_config)
{
    memset(wifi_config, 0, sizeof(*wifi_config));
    copy_text((char *)wifi_config->sta.ssid, sizeof(wifi_config->sta.ssid), net_config->ssid);
    copy_text((char *)wifi_config->sta.password, sizeof(wifi_config->sta.password), net_config->password);
    wifi_config->sta.scan_method = net_config->scan_method;
    wifi_config->sta.sort_method = net_config->sort_method;
    wifi_config->sta.threshold.rssi = net_config->threshold_rssi;
    wifi_config->sta.threshold.authmode = net_config->auth_mode_threshold;
}

/* Build the full interface config from station settings. */
static void fill_iface_config(net_iface_config_t *cfg, const net_wifi_sta_config_t *sta)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->type = NET_IFACE_TYPE_WIFI_STA;
    cfg->desc = NET_CONNECT_NETIF_DESC_STA;
    cfg->route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO;
#if CONFIG_NET_CONNECT_WIFI_REQUIRED
    cfg->required = true;
#else
    cfg->required = false;
#endif
    /* An empty name means "do not join yet". Credentials can be set later. */
    cfg->auto_connect_on_start = (sta->ssid[0] != '\0');
    cfg->ip = sta->ip;
    cfg->wifi_sta = *sta;
    cfg->wifi_sta.ip = cfg->ip;
}

/* The station has associated. Address events are handled in net_iface.c. */
static void on_wifi_sta_connected(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    (void)event_base;
    (void)event_id;
    (void)event_data;
    net_iface_notify_connected((net_iface_handle_t)arg);
}

/* The station lost the AP. Join again until max_retry is used up. */
static void on_wifi_sta_disconnected(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    (void)event_base;
    (void)event_id;
    net_iface_handle_t self = (net_iface_handle_t)arg;
    if (self == NULL || s_wifi_ignore_disconnect) {
        return;
    }

    wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
    if (disconn != NULL && disconn->reason == WIFI_REASON_ROAMING) {
        ESP_LOGD(TAG, "station roaming, do nothing");
        return;
    }

    xSemaphoreTake(self->lock, portMAX_DELAY);
    /* Stop and disconnect already own this event. Do not start another join. */
    if (self->state == NET_IFACE_STATE_STOPPING || self->state == NET_IFACE_STATE_INIT ||
            self->state == NET_IFACE_STATE_STARTED) {
        xSemaphoreGive(self->lock);
        return;
    }
    self->retry_num++;
    int retry_num = self->retry_num;
    int max_retry = self->cfg.wifi_sta.max_retry;
    xSemaphoreGive(self->lock);

    if (retry_num > max_retry) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", retry_num);
        net_iface_notify_failed(self, ESP_FAIL);
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect... (%d/%d)", retry_num, max_retry);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    }
}

/* Remove the station event handlers. Missing handlers are not an error. */
static void unregister_wifi_handlers(void)
{
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, on_wifi_sta_connected);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_wifi_sta_disconnected);
}

/* Turn the Wi-Fi driver on, create the STA netif, and report that startup finished. */
static esp_err_t wifi_ops_start(net_iface_handle_t self)
{
    if (s_wifi_netif != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init_cfg);
    bool we_inited = true;
    if (err == ESP_ERR_INVALID_STATE) {
        /* The radio was already initialized by someone else. */
        we_inited = false;
        err = ESP_OK;
    } else if (err != ESP_OK) {
        return err;
    }

    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base.if_desc = self->cfg.desc != NULL ? self->cfg.desc : NET_CONNECT_NETIF_DESC_STA;
    base.route_prio = self->cfg.route_prio > 0 ? self->cfg.route_prio : NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO;

    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_STA, &base);
    if (netif == NULL) {
        if (we_inited) {
            esp_wifi_deinit();
        }
        return ESP_ERR_NO_MEM;
    }

    esp_wifi_set_default_wifi_sta_handlers();

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        goto fail;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        goto fail;
    }
    err = esp_wifi_start();
    if (err == ESP_ERR_INVALID_STATE) {
        err = ESP_OK;
    }
    if (err != ESP_OK) {
        goto fail;
    }

    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                     on_wifi_sta_connected, self);
    if (err != ESP_OK) {
        goto fail_started;
    }
    err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                     on_wifi_sta_disconnected, self);
    if (err != ESP_OK) {
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, on_wifi_sta_connected);
        goto fail_started;
    }

    s_wifi_ignore_disconnect = false;
    s_wifi_netif = netif;
    net_iface_notify_started(self, netif);
    return ESP_OK;

fail_started:
    esp_wifi_stop();
fail:
    esp_wifi_clear_default_wifi_driver_and_handlers(netif);
    esp_netif_destroy(netif);
    if (we_inited) {
        esp_wifi_deinit();
    }
    return err != ESP_OK ? err : ESP_FAIL;
}

/*
 * Join the configured network.
 * Returns immediately. An empty SSID is not a failure of the driver: the
 * interface stays started until net_wifi_sta_set_credentials() is called.
 */
static esp_err_t wifi_ops_connect(net_iface_handle_t self)
{
    xSemaphoreTake(self->lock, portMAX_DELAY);
    net_wifi_sta_config_t sta = self->cfg.wifi_sta;
    self->retry_num = 0;
    xSemaphoreGive(self->lock);

    if (sta.ssid[0] == '\0') {
        ESP_LOGI(TAG, "Wi-Fi SSID is empty; staying in STARTED state until credentials are set");
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t wifi_config;
    convert_to_wifi_config(&sta, &wifi_config);
    s_wifi_ignore_disconnect = false;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    return esp_wifi_connect();
}

/* Drop the current association. The radio stays on. */
static esp_err_t wifi_ops_disconnect(net_iface_handle_t self)
{
    (void)self;
    s_wifi_ignore_disconnect = true;
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT &&
            err != ESP_ERR_WIFI_NOT_STARTED && err != ESP_FAIL) {
        ESP_LOGW(TAG, "esp_wifi_disconnect returned %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

/* Turn the radio off and destroy the STA netif. */
static esp_err_t wifi_ops_stop(net_iface_handle_t self)
{
    (void)self;
    s_wifi_ignore_disconnect = true;
    unregister_wifi_handlers();

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "esp_wifi_stop returned %s", esp_err_to_name(err));
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "esp_wifi_deinit returned %s", esp_err_to_name(err));
    }

    if (s_wifi_netif != NULL) {
        esp_wifi_clear_default_wifi_driver_and_handlers(s_wifi_netif);
        esp_netif_destroy(s_wifi_netif);
        s_wifi_netif = NULL;
    }
    return ESP_OK;
}

/* Forget the legacy pointer when this handle is destroyed. */
static esp_err_t wifi_ops_deinit(net_iface_handle_t self)
{
    if (s_wifi_sta_handle == self) {
        s_wifi_sta_handle = NULL;
    }
    return ESP_OK;
}

/* Driver callbacks used by net_iface_init() and net_wifi_sta_create(). */
const net_iface_ops_t g_net_iface_wifi_sta_ops = {
    .name = "wifi_sta",
    .init = NULL,
    .deinit = wifi_ops_deinit,
    .start = wifi_ops_start,
    .stop = wifi_ops_stop,
    .connect = wifi_ops_connect,
    .disconnect = wifi_ops_disconnect,
};

esp_err_t net_wifi_sta_create(const net_wifi_sta_params_t *params, net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;

    net_iface_config_t cfg = { 0 };
    net_wifi_sta_config_t sta;
    populate_config_from_kconfig(&sta);
    fill_iface_config(&cfg, &sta);
    cfg.ip.use_dhcp = true;
#if CONFIG_NET_CONNECT_IPV6
    cfg.ip.enable_ipv6 = true;
#else
    cfg.ip.enable_ipv6 = false;
#endif
    cfg.wifi_sta.ip = cfg.ip;

    if (params != NULL) {
        if (params->ssid != NULL) {
            copy_text(cfg.wifi_sta.ssid, sizeof(cfg.wifi_sta.ssid), params->ssid);
        }
        if (params->password != NULL) {
            copy_text(cfg.wifi_sta.password, sizeof(cfg.wifi_sta.password), params->password);
        }
        if (params->max_retry >= 0) {
            cfg.wifi_sta.max_retry = params->max_retry;
        }
        if (params->route_prio >= 0) {
            cfg.route_prio = params->route_prio;
        }
        cfg.required = params->required;
        /*
         * The caller's flag is the one we keep. An empty name with the flag
         * left false stays idle until credentials are stored.
         */
        cfg.auto_connect_on_start = params->auto_connect_on_start;
        if (params->ssid != NULL && params->ssid[0] == '\0' && !params->auto_connect_on_start) {
            cfg.auto_connect_on_start = false;
        }
        if (params->ip != NULL) {
            cfg.ip = *params->ip;
            cfg.wifi_sta.ip = *params->ip;
        }
    } else if (cfg.wifi_sta.ssid[0] == '\0') {
        /* Kconfig has no name yet (for example the stdin prompt). Do not join. */
        cfg.auto_connect_on_start = false;
    }

    return net_iface_init_with_ops(&cfg, &g_net_iface_wifi_sta_ops, out_handle);
}

esp_err_t net_wifi_sta_set_credentials(net_iface_handle_t h, const char *ssid, const char *password)
{
    if (h == NULL || h->cfg.type != NET_IFACE_TYPE_WIFI_STA || ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (h->state != NET_IFACE_STATE_INIT &&
            h->state != NET_IFACE_STATE_STARTED &&
            h->state != NET_IFACE_STATE_FAILED) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    copy_text(h->cfg.wifi_sta.ssid, sizeof(h->cfg.wifi_sta.ssid), ssid);
    copy_text(h->cfg.wifi_sta.password, sizeof(h->cfg.wifi_sta.password),
              password != NULL ? password : "");
    xSemaphoreGive(h->lock);
    return ESP_OK;
}

#if CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
esp_err_t net_connect_wifi_apply_stdin(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Blocking console read stays here, never inside wifi_ops_connect(). */
    net_configure_stdin_stdout();
    /* Room for an SSID, a space, a password, and the newline. */
    char buf[32 + 64 + 2] = { 0 };
    ESP_LOGI(TAG, "Please input ssid password:");
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        ESP_LOGE(TAG, "Failed to read SSID/password from stdin");
        return ESP_ERR_INVALID_STATE;
    }
    buf[strcspn(buf, "\n")] = '\0';

    char *rest = NULL;
    char *ssid = strtok_r(buf, " ", &rest);
    if (ssid == NULL || ssid[0] == '\0') {
        ESP_LOGE(TAG, "SSID is empty or invalid");
        return ESP_ERR_INVALID_ARG;
    }
    char *password = strtok_r(NULL, " ", &rest);
    esp_err_t err = net_wifi_sta_set_credentials(h, ssid, password);
    if (err != ESP_OK) {
        return err;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (password == NULL || password[0] == '\0') {
        h->cfg.wifi_sta.auth_mode_threshold = WIFI_AUTH_OPEN;
    }
    h->cfg.auto_connect_on_start = true;
    xSemaphoreGive(h->lock);
    return ESP_OK;
}
#endif /* CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN */

/* How long the legacy connect wrapper waits. 0 in Kconfig means "no limit". */
static uint32_t legacy_wait_timeout_ms(void)
{
    return CONFIG_NET_CONNECT_TIMEOUT_MS == 0 ? UINT32_MAX : (uint32_t)CONFIG_NET_CONNECT_TIMEOUT_MS;
}

net_iface_handle_t net_configure_wifi_sta(net_wifi_sta_config_t *config)
{
    ESP_LOGI(TAG, "Configuring Wi-Fi STA interface...");

    net_wifi_sta_config_t sta;
    if (config == NULL) {
        populate_config_from_kconfig(&sta);
    } else {
        if (config->ssid[0] == '\0') {
            ESP_LOGE(TAG, "STA SSID is empty");
            return NULL;
        }
        sta = *config;
    }

    net_iface_config_t cfg;
    fill_iface_config(&cfg, &sta);

    if (s_wifi_sta_handle == NULL) {
        s_wifi_sta_handle = net_iface_get_by_type(NET_IFACE_TYPE_WIFI_STA);
    }
    if (s_wifi_sta_handle != NULL) {
        xSemaphoreTake(s_wifi_sta_handle->lock, portMAX_DELAY);
        if (s_wifi_sta_handle->state != NET_IFACE_STATE_INIT &&
                s_wifi_sta_handle->state != NET_IFACE_STATE_STARTED &&
                s_wifi_sta_handle->state != NET_IFACE_STATE_FAILED) {
            xSemaphoreGive(s_wifi_sta_handle->lock);
            ESP_LOGE(TAG, "Cannot change Wi-Fi settings while the interface is busy");
            return NULL;
        }
        s_wifi_sta_handle->cfg.wifi_sta = cfg.wifi_sta;
        s_wifi_sta_handle->cfg.ip = cfg.ip;
        s_wifi_sta_handle->cfg.route_prio = cfg.route_prio;
        s_wifi_sta_handle->cfg.required = cfg.required;
        s_wifi_sta_handle->cfg.auto_connect_on_start = cfg.auto_connect_on_start;
        xSemaphoreGive(s_wifi_sta_handle->lock);
        return s_wifi_sta_handle;
    }

    net_iface_handle_t created = NULL;
    if (net_iface_init_with_ops(&cfg, &g_net_iface_wifi_sta_ops, &created) != ESP_OK) {
        return NULL;
    }
    s_wifi_sta_handle = created;
    return created;
}

esp_err_t net_connect_wifi(void)
{
    ESP_LOGI(TAG, "Connecting configured Wi-Fi interfaces...");
    if (s_wifi_sta_handle == NULL) {
        ESP_LOGE(TAG, "Wi-Fi STA not configured. Call net_configure_wifi_sta() first");
        return ESP_ERR_INVALID_STATE;
    }

    net_iface_handle_t h = s_wifi_sta_handle;

#if CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN
    esp_err_t err = net_connect_wifi_apply_stdin(h);
    if (err != ESP_OK) {
        return err;
    }
#else
    esp_err_t err = ESP_OK;
#endif

    net_iface_state_t state = net_iface_get_state(h);
    bool start_scheduled = false;
    if (state == NET_IFACE_STATE_INIT || state == NET_IFACE_STATE_FAILED) {
        /* The startup task connects once the radio is up. */
        xSemaphoreTake(h->lock, portMAX_DELAY);
        h->cfg.auto_connect_on_start = true;
        xSemaphoreGive(h->lock);
        err = net_iface_start(h);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
        start_scheduled = (err == ESP_OK);
    }

    /*
     * If start() was just scheduled, the startup task calls connect().
     * Connect here only when the interface was already up.
     */
    state = net_iface_get_state(h);
    if (!start_scheduled &&
            (state == NET_IFACE_STATE_STARTED || state == NET_IFACE_STATE_FAILED)) {
        err = net_iface_connect(h);
        if (err != ESP_OK) {
            return err;
        }
    }

    err = net_iface_wait(h, legacy_wait_timeout_ms());
    if (err != ESP_OK) {
        net_iface_stop(h);
        return err;
    }
    return ESP_OK;
}

esp_err_t net_disconnect_wifi(void)
{
    ESP_LOGI(TAG, "Disconnecting Wi-Fi interfaces...");
    net_iface_handle_t h = s_wifi_sta_handle;
    if (h == NULL) {
        h = net_iface_get_by_type(NET_IFACE_TYPE_WIFI_STA);
    }
    if (h == NULL) {
        return ESP_OK;
    }
    net_iface_stop(h);
    return net_iface_deinit(h);
}

bool net_connect_wifi_is_configured(void)
{
    return s_wifi_sta_handle != NULL;
}

#endif /* CONFIG_NET_CONNECT_WIFI */
