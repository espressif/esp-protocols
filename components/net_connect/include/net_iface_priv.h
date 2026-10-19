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
 * @brief Driver operations table implemented by each interface type.
 */
typedef struct net_iface_ops {
    const char *name;
    esp_err_t (*init)(net_iface_handle_t self);
    esp_err_t (*deinit)(net_iface_handle_t self);
    esp_err_t (*start)(net_iface_handle_t self);
    esp_err_t (*stop)(net_iface_handle_t self);
    esp_err_t (*connect)(net_iface_handle_t self);
    esp_err_t (*disconnect)(net_iface_handle_t self);
} net_iface_ops_t;

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
    bool                   cancel_requested; /*!< Set by net_iface_stop() to abort in-flight startup */
    int                    retry_num;        /*!< Connection retry counter */
    void                  *drv;              /*!< Driver-private state pointer */
    struct net_iface      *next;             /*!< Next pointer in global registry list */
};

/* ============================================================
 *       Driver-to-Core State Notification Helpers
 * ============================================================ */

void net_iface_notify_started(net_iface_handle_t h, esp_netif_t *netif);
void net_iface_notify_connected(net_iface_handle_t h);
void net_iface_notify_ready(net_iface_handle_t h);
void net_iface_notify_lost_ip(net_iface_handle_t h);
void net_iface_notify_disconnected(net_iface_handle_t h);
void net_iface_notify_failed(net_iface_handle_t h, esp_err_t err);
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
