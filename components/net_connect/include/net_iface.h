/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"
#if !CONFIG_IDF_TARGET_LINUX
#include "esp_event.h"
#include "esp_netif.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle representing a managed network interface instance.
 */
typedef struct net_iface *net_iface_handle_t;

/**
 * @brief Supported network interface types.
 */
typedef enum {
    NET_IFACE_TYPE_WIFI_STA = 0, /*!< Wi-Fi Station (STA) interface */
    NET_IFACE_TYPE_ETH,          /*!< Ethernet interface */
    NET_IFACE_TYPE_THREAD,       /*!< OpenThread IEEE 802.15.4 interface */
    NET_IFACE_TYPE_PPP,          /*!< Point-to-Point Protocol (PPP over UART/USB) interface */
    NET_IFACE_TYPE_MAX
} net_iface_type_t;

/**
 * @brief Lifecycle and readiness states of a network interface.
 *
 * Note on readiness:
 * - NET_IFACE_STATE_CONNECTED means the physical or L2 link is up.
 * - NET_IFACE_STATE_READY means the interface has obtained its required IP configuration
 *   (IPv4 and/or preferred IPv6 for Wi-Fi, Ethernet, and PPP; OPENTHREAD_EVENT_ATTACHED
 *   for OpenThread). It does NOT guarantee end-to-end internet reachability.
 */
typedef enum {
    NET_IFACE_STATE_UNINIT = 0, /*!< Uninitialized or deinitialized */
    NET_IFACE_STATE_INIT,       /*!< Handle created and config stored; hardware not touched */
    NET_IFACE_STATE_STARTING,   /*!< Background startup task is initializing driver and netif */
    NET_IFACE_STATE_STARTED,    /*!< Driver and esp_netif created; link not requested yet */
    NET_IFACE_STATE_CONNECTING, /*!< Association, autonegotiation, PPP, or Thread attach in progress */
    NET_IFACE_STATE_CONNECTED,  /*!< L2 link is up; waiting for required IP address(es) */
    NET_IFACE_STATE_READY,      /*!< Required IP configuration (or Thread attach) acquired */
    NET_IFACE_STATE_STOPPING,   /*!< Shutdown in progress; waiting for worker/startup task exit */
    NET_IFACE_STATE_FAILED      /*!< Driver startup failed or max connection retries exceeded */
} net_iface_state_t;

/* Default configuration constants */
#define NET_CONNECT_DEFAULT_USE_DHCP        true
#define NET_CONNECT_DEFAULT_ENABLE_IPV6     false
#define NET_CONNECT_WIFI_DEFAULT_MAX_RETRY  5
#define NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO 128
#define NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO  64
#define NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO  32
#define NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO 16

/**
 * @brief IP configuration reusable across interface types (IPv4 and IPv6).
 */
typedef struct {
    /* IPv4 Configuration */
    bool use_dhcp;                /*!< true = use DHCP client, false = use static IPv4 */
    char ip[16];                  /*!< Static IPv4 address string (when use_dhcp == false) */
    char netmask[16];             /*!< Static IPv4 subnet mask string */
    char gateway[16];             /*!< Static IPv4 gateway address string */
    char dns_main[16];            /*!< Primary IPv4 DNS server */
    char dns_backup[16];          /*!< Secondary IPv4 DNS server */
    char dns_fallback[16];        /*!< Fallback IPv4 DNS server */

    /* IPv6 Configuration */
    bool enable_ipv6;             /*!< true = wait for preferred IPv6 address before READY */
    int ipv6_mode;                /*!< 0 = SLAAC, 1 = Manual, 2 = DHCPv6 */
    char ipv6_addr[40];           /*!< Static IPv6 address (when ipv6_mode == 1) */
    char ipv6_gateway[40];        /*!< Static IPv6 gateway address */
    char ipv6_dns_main[40];       /*!< Primary IPv6 DNS server */
    char ipv6_dns_backup[40];     /*!< Secondary IPv6 DNS server */
} net_ip_config_t;

/**
 * @brief Wi-Fi Station (STA) driver configuration.
 */
typedef struct {
    char ssid[32];                /*!< Target AP SSID (empty string allowed if provisioned later) */
    char password[64];            /*!< Target AP password */
    int scan_method;              /*!< WIFI_FAST_SCAN or WIFI_ALL_CHANNEL_SCAN */
    int sort_method;              /*!< WIFI_CONNECT_AP_BY_SIGNAL or WIFI_CONNECT_AP_BY_SECURITY */
    int threshold_rssi;           /*!< Minimum RSSI threshold */
    int auth_mode_threshold;      /*!< Weakest acceptable authentication mode */
    int max_retry;                /*!< Maximum connection retries before entering FAILED state */
    net_ip_config_t ip;           /*!< IP settings (kept inside wifi_sta for backward compatibility) */
} net_wifi_sta_config_t;

/**
 * @brief Ethernet driver configuration.
 */
typedef struct {
    int placeholder;              /*!< Board PHY/MAC selection comes from ethernet_init Kconfig */
} net_eth_config_t;

/**
 * @brief OpenThread driver configuration.
 */
typedef struct {
    int task_stack_size;          /*!< OpenThread worker task stack size in bytes */
} net_thread_config_t;

/**
 * @brief PPP driver configuration.
 */
typedef struct {
    int max_retry;                /*!< Maximum reconnect attempts on IP_EVENT_PPP_LOST_IP */
    int task_stack_size;          /*!< PPP UART task stack size in bytes */
} net_ppp_config_t;

/**
 * @brief Unified configuration structure for net_iface_init().
 */
typedef struct {
    net_iface_type_t type;        /*!< Interface type */
    const char *desc;             /*!< Custom netif description, or NULL for default NET_CONNECT_NETIF_DESC_* */
    int route_prio;               /*!< Initial esp_netif routing priority (higher wins default route) */
    bool required;                /*!< true if net_connect() must fail when this interface fails */
    bool auto_connect_on_start;   /*!< true if net_iface_start() should automatically begin connecting */
    int startup_task_stack_size;  /*!< Stack size for transient startup task (0 = Kconfig default) */
    net_ip_config_t ip;           /*!< Common IPv4/IPv6 settings */
    union {
        net_wifi_sta_config_t wifi_sta;
        net_eth_config_t      eth;
        net_thread_config_t   thread;
        net_ppp_config_t      ppp;
    };
} net_iface_config_t;

/* ============================================================
 *              Typed Creation Wrapper Parameters
 * ============================================================ */

/**
 * @brief Simplified parameters for net_wifi_sta_create().
 * Pass NULL to net_wifi_sta_create() to use all Kconfig defaults.
 */
typedef struct {
    const char *ssid;             /*!< NULL = use CONFIG_NET_CONNECT_WIFI_SSID; "" = unprovisioned */
    const char *password;         /*!< NULL = use CONFIG_NET_CONNECT_WIFI_PASSWORD */
    int max_retry;                /*!< < 0 = use CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY */
    int route_prio;               /*!< < 0 = use NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO (128) */
    bool required;                /*!< Whether net_connect() requires this interface */
    bool auto_connect_on_start;   /*!< true to connect immediately on start (when SSID is non-empty) */
    const net_ip_config_t *ip;    /*!< NULL = use DHCP / Kconfig IPv6 defaults */
} net_wifi_sta_params_t;

/**
 * @brief Simplified parameters for net_eth_create().
 */
typedef struct {
    int route_prio;               /*!< < 0 = use NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO (64) */
    bool required;                /*!< Whether net_connect() requires this interface */
    bool auto_connect_on_start;   /*!< true to wait for link/IP automatically on start */
    const net_ip_config_t *ip;    /*!< NULL = use DHCP / Kconfig IPv6 defaults */
} net_eth_params_t;

/**
 * @brief Simplified parameters for net_thread_create().
 */
typedef struct {
    int task_stack_size;          /*!< <= 0 = use CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE */
    int route_prio;               /*!< < 0 = use NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO (16) */
    bool required;                /*!< Whether net_connect() requires this interface */
    bool auto_connect_on_start;   /*!< true to run esp_openthread_auto_start() on start */
    const net_ip_config_t *ip;    /*!< NULL = default IPv6 settings */
} net_thread_params_t;

/**
 * @brief Simplified parameters for net_ppp_create().
 */
typedef struct {
    int max_retry;                /*!< < 0 = use CONFIG_NET_CONNECT_PPP_CONN_MAX_RETRY */
    int task_stack_size;          /*!< <= 0 = use CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE */
    int route_prio;               /*!< < 0 = use NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO (32) */
    bool required;                /*!< Whether net_connect() requires this interface */
    bool auto_connect_on_start;   /*!< true to start PPP negotiation on start */
    const net_ip_config_t *ip;    /*!< NULL = use default PPP IP settings */
} net_ppp_params_t;

#if !CONFIG_IDF_TARGET_LINUX

/* ============================================================
 *                    Core Lifecycle API
 * ============================================================ */

/**
 * @brief Initialize a network interface handle from a full configuration struct.
 *
 * Performs one-time global initialization (esp_netif_init and default event loop)
 * if not already done, validates @p cfg, allocates the handle in NET_IFACE_STATE_INIT,
 * and registers it. Does not touch hardware or start any driver.
 *
 * @param[in]  cfg        Configuration structure (must not be NULL).
 * @param[out] out_handle Returned interface handle (must not be NULL).
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if cfg or out_handle is NULL or invalid
 *  - ESP_ERR_NO_MEM if allocation fails
 */
esp_err_t net_iface_init(const net_iface_config_t *cfg, net_iface_handle_t *out_handle);

/**
 * @brief Deinitialize and free an interface handle.
 *
 * The interface must be in NET_IFACE_STATE_INIT (call net_iface_stop() first if started).
 *
 * @param[in] h Interface handle.
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if h is NULL
 *  - ESP_ERR_INVALID_STATE if the interface is currently started or starting
 */
esp_err_t net_iface_deinit(net_iface_handle_t h);

/**
 * @brief Start the interface driver in the background (non-blocking).
 *
 * Transitions the interface from INIT (or FAILED) to STARTING and schedules the
 * per-interface startup task. Returns immediately without waiting for hardware
 * initialization, PHY autonegotiation, or IP acquisition.
 *
 * Note: ESP_OK means startup was scheduled. If the driver later fails to initialize,
 * the interface transitions to NET_IFACE_STATE_FAILED, stores the error code for
 * net_iface_get_last_error(), and posts NET_IFACE_EVENT_FAILED.
 *
 * @param[in] h Interface handle.
 * @return
 *  - ESP_OK if startup was scheduled
 *  - ESP_ERR_INVALID_ARG if h is NULL
 *  - ESP_ERR_INVALID_STATE if already starting or started
 *  - ESP_ERR_NO_MEM if the startup task could not be created
 */
esp_err_t net_iface_start(net_iface_handle_t h);

/**
 * @brief Stop the interface driver and destroy its esp_netif.
 *
 * Safe to call from STARTING, STARTED, CONNECTING, CONNECTED, READY, or FAILED.
 * If called while STARTING, requests cancellation and waits for the startup task
 * to finish before tearing down resources. Transitions the handle back to INIT.
 * Must NOT be called from inside a NET_IFACE_EVENT callback on the event loop task.
 *
 * @param[in] h Interface handle.
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if h is NULL
 *  - ESP_ERR_INVALID_STATE if called from the default event loop task
 */
esp_err_t net_iface_stop(net_iface_handle_t h);

/**
 * @brief Initiate network connection on a started interface (non-blocking).
 *
 * Returns immediately after triggering association/negotiation.
 *
 * @param[in] h Interface handle (must be in STARTED, DISCONNECTED, or FAILED state).
 * @return
 *  - ESP_OK if connection attempt was started
 *  - ESP_ERR_INVALID_ARG if h is NULL
 *  - ESP_ERR_INVALID_STATE if the interface is not started or lacks required credentials
 */
esp_err_t net_iface_connect(net_iface_handle_t h);

/**
 * @brief Disconnect an active or connecting link while keeping the driver started.
 *
 * Clears readiness and transitions the interface back to STARTED.
 *
 * @param[in] h Interface handle.
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if h is NULL
 *  - ESP_ERR_INVALID_STATE if the interface is not started
 */
esp_err_t net_iface_disconnect(net_iface_handle_t h);

/* ============================================================
 *                  Blocking Readiness Wait APIs
 * ============================================================ */

/**
 * @brief Wait for a single interface to reach NET_IFACE_STATE_READY.
 *
 * - If the interface is already READY, returns ESP_OK immediately.
 * - If the interface is already FAILED, returns its last_err immediately.
 * - If timeout_ms expires first, returns ESP_ERR_TIMEOUT and leaves startup/connection
 *   running in the background.
 * - Must NOT be called from the default event loop task (returns ESP_ERR_INVALID_STATE).
 *
 * @param[in] h          Interface handle.
 * @param[in] timeout_ms Timeout in milliseconds (or UINT32_MAX to wait indefinitely).
 * @return
 *  - ESP_OK when READY
 *  - ESP_ERR_TIMEOUT on timeout
 *  - ESP_ERR_INVALID_ARG if h is NULL
 *  - ESP_ERR_INVALID_STATE if called from the event loop task or interface is INIT/STOPPED
 *  - Interface error code if the interface entered FAILED
 */
esp_err_t net_iface_wait(net_iface_handle_t h, uint32_t timeout_ms);

/**
 * @brief Wait for ALL specified interfaces to reach NET_IFACE_STATE_READY within a shared timeout.
 *
 * Fails fast and returns immediately if any handle in @p handles enters NET_IFACE_STATE_FAILED.
 *
 * @param[in] handles    Array of interface handles.
 * @param[in] count      Number of handles in @p handles (must be > 0).
 * @param[in] timeout_ms Shared overall timeout budget in milliseconds.
 * @return
 *  - ESP_OK when all handles are READY
 *  - First failing handle's error code if any handle fails
 *  - ESP_ERR_TIMEOUT if the deadline expires before all handles are READY
 *  - ESP_ERR_INVALID_ARG or ESP_ERR_INVALID_STATE on invalid call
 */
esp_err_t net_iface_wait_all(const net_iface_handle_t *handles, size_t count, uint32_t timeout_ms);

/**
 * @brief Wait for ANY of the specified interfaces to reach NET_IFACE_STATE_READY.
 *
 * Returns ESP_OK as soon as at least one handle in @p handles is READY. If all handles
 * in @p handles enter NET_IFACE_STATE_FAILED, fails fast and returns the first error.
 *
 * @param[in]  handles    Array of interface handles.
 * @param[in]  count      Number of handles in @p handles (must be > 0).
 * @param[in]  timeout_ms Shared overall timeout budget in milliseconds.
 * @param[out] out_ready  Optional pointer receiving the handle that became READY (may be NULL).
 * @return
 *  - ESP_OK when at least one handle is READY
 *  - Error code if all handles entered FAILED
 *  - ESP_ERR_TIMEOUT if the deadline expires before any handle is READY
 *  - ESP_ERR_INVALID_ARG or ESP_ERR_INVALID_STATE on invalid call
 */
esp_err_t net_iface_wait_any(const net_iface_handle_t *handles, size_t count,
                             uint32_t timeout_ms, net_iface_handle_t *out_ready);

/* ============================================================
 *                   Typed Creation Wrappers
 * ============================================================ */

esp_err_t net_wifi_sta_create(const net_wifi_sta_params_t *params, net_iface_handle_t *out_handle);
esp_err_t net_eth_create(const net_eth_params_t *params, net_iface_handle_t *out_handle);
esp_err_t net_thread_create(const net_thread_params_t *params, net_iface_handle_t *out_handle);
esp_err_t net_ppp_create(const net_ppp_params_t *params, net_iface_handle_t *out_handle);

/**
 * @brief Set or update Wi-Fi STA credentials before connecting (for runtime provisioning).
 *
 * Allowed when the Wi-Fi handle is in NET_IFACE_STATE_INIT, NET_IFACE_STATE_STARTED,
 * or NET_IFACE_STATE_FAILED.
 *
 * @param[in] h        Wi-Fi STA interface handle.
 * @param[in] ssid     SSID string (must be non-NULL and non-empty).
 * @param[in] password Password string (NULL or "" for open network).
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if h is not a Wi-Fi STA handle or ssid is empty
 *  - ESP_ERR_INVALID_STATE if the interface is currently CONNECTING, CONNECTED, or READY
 */
esp_err_t net_wifi_sta_set_credentials(net_iface_handle_t h, const char *ssid, const char *password);

/* ============================================================
 *                    Getters and Iteration
 * ============================================================ */

typedef bool (*net_iface_cb_t)(net_iface_handle_t h, void *ctx);

esp_netif_t       *net_iface_get_netif(net_iface_handle_t h);
net_iface_state_t  net_iface_get_state(net_iface_handle_t h);
net_iface_type_t   net_iface_get_type(net_iface_handle_t h);
esp_err_t          net_iface_get_last_error(net_iface_handle_t h);
net_iface_handle_t net_iface_get_by_type(net_iface_type_t type);
net_iface_handle_t net_iface_get_by_desc(const char *desc);
esp_err_t          net_iface_for_each(net_iface_cb_t cb, void *ctx);

/* ============================================================
 *                  Events and Notifications
 * ============================================================ */

ESP_EVENT_DECLARE_BASE(NET_IFACE_EVENT);

/**
 * @brief Event IDs posted under NET_IFACE_EVENT on the default ESP event loop.
 *
 * Callbacks execute on the default event loop task and must return quickly.
 * Do NOT call net_iface_wait*() or net_iface_stop() inside an event callback.
 * Registering a handler does not replay past events; call net_iface_get_state()
 * after registration to inspect interfaces that may already be READY or FAILED.
 */
typedef enum {
    NET_IFACE_EVENT_STARTED = 0,  /*!< Driver and esp_netif created */
    NET_IFACE_EVENT_CONNECTED,    /*!< Physical / L2 link up */
    NET_IFACE_EVENT_READY,        /*!< Required IP configuration (or Thread attach) obtained */
    NET_IFACE_EVENT_LOST_IP,      /*!< IP address lost; interface is no longer READY */
    NET_IFACE_EVENT_DISCONNECTED, /*!< Physical / L2 link disconnected */
    NET_IFACE_EVENT_STOPPED,      /*!< Interface stopped and esp_netif destroyed */
    NET_IFACE_EVENT_FAILED        /*!< Startup or connection failed; see data->err */
} net_iface_event_id_t;

/**
 * @brief Event payload passed as event_data for all NET_IFACE_EVENT notifications.
 */
typedef struct {
    net_iface_handle_t iface;     /*!< Interface handle that generated the event */
    esp_netif_t       *netif;     /*!< Associated esp_netif pointer (may be NULL on early FAILED/STOPPED) */
    net_iface_type_t   type;      /*!< Interface type */
    net_iface_state_t  state;     /*!< Current state at the time of notification */
    esp_err_t          err;       /*!< Error code for NET_IFACE_EVENT_FAILED, otherwise ESP_OK */
} net_iface_event_data_t;

esp_err_t net_iface_register_event_handler(esp_event_handler_t handler, void *arg);
esp_err_t net_iface_unregister_event_handler(esp_event_handler_t handler);

#endif /* !CONFIG_IDF_TARGET_LINUX */

#ifdef __cplusplus
}
#endif
