/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "net_iface.h"

#if !CONFIG_IDF_TARGET_LINUX
#include "esp_bit_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !CONFIG_IDF_TARGET_LINUX

/* Sticky event-group bits stored in iface->state_events */
#define NET_IFACE_BIT_READY        BIT0 /*!< Sticky bit: interface is in NET_IFACE_STATE_READY */
#define NET_IFACE_BIT_FAILED       BIT1 /*!< Sticky bit: interface is in NET_IFACE_STATE_FAILED */
#define NET_IFACE_BIT_STOPPED      BIT2 /*!< Sticky bit: interface is stopped / back in INIT */
#define NET_IFACE_BIT_IPV4         BIT3 /*!< IPv4 address acquired on this interface */
#define NET_IFACE_BIT_IPV6         BIT4 /*!< Preferred IPv6 address acquired on this interface */
#define NET_IFACE_BIT_OT_ATTACHED  BIT5 /*!< OpenThread attached event received */
#define NET_IFACE_BIT_STARTUP_DONE BIT6 /*!< Transient startup task has finished executing */

/**
 * @brief Callbacks implemented by one interface driver.
 *
 * start and connect return as soon as the work is kicked off.
 * They do not wait for an IP address or for Thread to attach.
 */
typedef struct net_iface_ops {
    const char *name;                                 /*!< Short name for logs */
    esp_err_t (*init)(net_iface_handle_t self);       /*!< Optional. Save settings only. Do not touch hardware. */
    esp_err_t (*deinit)(net_iface_handle_t self);     /*!< Optional. Called when the handle is freed. */
    esp_err_t (*start)(net_iface_handle_t self);      /*!< Turn the driver on and report that it has started. */
    esp_err_t (*stop)(net_iface_handle_t self);       /*!< Turn the driver off and free its netif. */
    esp_err_t (*connect)(net_iface_handle_t self);    /*!< Begin the link. Return without waiting for an address. */
    esp_err_t (*disconnect)(net_iface_handle_t self); /*!< Drop the link. Leave the driver running. */
} net_iface_ops_t;

/*
 * Each driver file defines the table for its interface.
 * net_iface_init() uses these to pick the right driver.
 */
#if CONFIG_NET_CONNECT_WIFI
extern const net_iface_ops_t g_net_iface_wifi_sta_ops;
#endif
#if CONFIG_NET_CONNECT_ETHERNET
extern const net_iface_ops_t g_net_iface_eth_ops;
#endif
#if CONFIG_NET_CONNECT_THREAD
extern const net_iface_ops_t g_net_iface_thread_ops;
#endif
#if CONFIG_NET_CONNECT_PPP
extern const net_iface_ops_t g_net_iface_ppp_ops;
#endif

/**
 * @brief Internal representation of a network interface instance.
 */
struct net_iface {
    const net_iface_ops_t *ops;              /*!< Driver callbacks */
    net_iface_config_t     cfg;              /*!< Copy of interface configuration */
    net_iface_state_t      state;            /*!< Current lifecycle state */
    esp_err_t              last_err;         /*!< Last error code when in FAILED state */
    esp_netif_t           *netif;            /*!< Underlying esp_netif instance */
    SemaphoreHandle_t      lock;             /*!< Per-interface mutex (acquired after registry mutex) */
    EventGroupHandle_t     state_events;     /*!< Sticky readiness/failure bits for wait APIs */
    TaskHandle_t           startup_task;     /*!< Handle of running startup/worker task, or NULL */
    uint32_t               startup_stack_hwm;/*!< Minimum free stack bytes left in the startup task */
    bool                   cancel_requested; /*!< Set by net_iface_stop() to abort in-flight startup */
    int                    retry_num;        /*!< Connection retry counter */
    void                  *drv;              /*!< Driver-private state pointer */
    struct net_iface      *next;             /*!< Next pointer in global registry list */
};

/* ============================================================
 *       Driver-to-Core State Notification Helpers
 * ============================================================ */

/* The driver is up and netif can be used. Ignored during stop. */
void net_iface_notify_started(net_iface_handle_t h, esp_netif_t *netif);

/* The link is up. An address may still be missing. */
void net_iface_notify_connected(net_iface_handle_t h);

/* The address (or Thread attach) this interface needs is ready. */
void net_iface_notify_ready(net_iface_handle_t h);

/* The address went away. The link may still be up. */
void net_iface_notify_lost_ip(net_iface_handle_t h);

/* The link dropped. The driver stays running. */
void net_iface_notify_disconnected(net_iface_handle_t h);

/* Startup or connection failed. err is saved for net_iface_wait(). */
void net_iface_notify_failed(net_iface_handle_t h, esp_err_t err);

/* The driver is off and the netif is gone. The handle is idle again. */
void net_iface_notify_stopped(net_iface_handle_t h);

/**
 * @brief Initialize an interface with a custom ops table (used by drivers and unit test mocks).
 */
esp_err_t net_iface_init_with_ops(const net_iface_config_t *cfg,
                                  const net_iface_ops_t *ops,
                                  net_iface_handle_t *out_handle);

#endif /* !CONFIG_IDF_TARGET_LINUX */

#ifdef __cplusplus
}
#endif
