/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "net_iface.h"
#include "net_iface_priv.h"
#include "net_connect_private.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#if CONFIG_NET_CONNECT_THREAD
#include "esp_openthread_types.h"
#endif

/*
 * This file keeps one shared record of every network interface.
 *
 * Wi-Fi, Ethernet, Thread, and PPP do not each track "do we have an address
 * yet?" on their own. They tell this file what happened (started, connected,
 * failed, and so on). IP address events and OpenThread events also arrive
 * here. From those, this file updates three things:
 *   - the interface state (starting, connected, ready, failed, ...)
 *   - a set of flags that net_iface_wait() can sleep on
 *   - a notification on the default event loop, so an application can listen
 *
 * Two locks are used. The list lock guards the list of interfaces. Each
 * interface also has its own lock. If both are needed, take the list lock
 * first, then the interface lock.
 *
 * Do not hold either lock while calling the driver (start, stop, connect, or
 * disconnect) or while posting or registering an event. Those calls can run
 * application code, and that code may need the same locks.
 *
 * The "ready" and "failed" flags stay set until something clears them. A wait
 * that starts after the flag is already set returns right away, and two tasks
 * waiting on the same interface both wake up. The flags are cleared when the
 * IP is lost, the link drops, the interface stops, or a new start or connect
 * begins.
 */

static const char *TAG = "net_iface";

ESP_EVENT_DEFINE_BASE(NET_IFACE_EVENT);

/* Every flag that a new start, or a stop, wipes clean. */
#define NET_IFACE_BITS_LIVENESS (NET_IFACE_BIT_READY | NET_IFACE_BIT_FAILED | \
                                 NET_IFACE_BIT_STOPPED | NET_IFACE_BIT_IPV4 | \
                                 NET_IFACE_BIT_IPV6 | NET_IFACE_BIT_OT_ATTACHED)

/* Lock for the interface list. Take this before an interface's own lock. */
static SemaphoreHandle_t s_registry_mutex = NULL;
/* First interface in the list. Empty when nothing has been created. */
static net_iface_handle_t s_iface_list = NULL;
/* True after the shared IP handler (and the Thread handler, if built) is installed. */
static bool s_global_handlers_registered = false;
/*
 * The task that last handled a NET_IFACE, IP, or Thread event. That task is
 * the default event loop. net_iface_wait() and net_iface_stop() refuse to run
 * on it. If they blocked there, the event that should wake them could never arrive.
 */
static TaskHandle_t s_event_loop_task = NULL;

/* Bit pulsed whenever any interface becomes ready, failed, or stopped. */
#define NET_IFACE_GLOBAL_WAKE_BIT BIT0

/*
 * Shared wake-up for net_iface_wait_all() and net_iface_wait_any().
 * Each interface still keeps its own sticky flags. This group only says
 * "look again". Created with the list lock and freed when the last
 * interface is destroyed.
 */
static EventGroupHandle_t s_global_wait_events = NULL;

static void on_global_iface_event(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data);
static void on_global_ip_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
#if CONFIG_NET_CONNECT_THREAD
static void on_global_ot_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
#endif

/* True when this function was called from the event-loop task. */
static bool called_from_event_loop(void)
{
    /* No event has run yet, so this cannot be the event-loop task. */
    return s_event_loop_task != NULL && xTaskGetCurrentTaskHandle() == s_event_loop_task;
}

/* Wake any task sleeping in net_iface_wait_all() or net_iface_wait_any(). */
static void pulse_global_wait_events(void)
{
    if (s_global_wait_events != NULL) {
        xEventGroupSetBits(s_global_wait_events, NET_IFACE_GLOBAL_WAKE_BIT);
    }
}

/*
 * Remember the event-loop task before any application NET_IFACE_EVENT
 * callback runs. Registered when the first interface is created, which is
 * before an application handler, so this runs first.
 */
static void on_global_iface_event(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_id;
    (void)event_data;
    s_event_loop_task = xTaskGetCurrentTaskHandle();
}

/* Create the list lock the first time an interface is created. Later calls reuse it. */
static esp_err_t ensure_registry_mutex(void)
{
    bool created_mutex = false;
    if (s_registry_mutex == NULL) {
        s_registry_mutex = xSemaphoreCreateMutex();
        if (s_registry_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
        created_mutex = true;
    }
    if (s_global_wait_events == NULL) {
        s_global_wait_events = xEventGroupCreate();
        if (s_global_wait_events == NULL) {
            if (created_mutex) {
                vSemaphoreDelete(s_registry_mutex);
                s_registry_mutex = NULL;
            }
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

/*
 * Free the list lock when the last interface is gone. Unit tests check that
 * memory returns to the starting amount, so the lock cannot be left behind.
 * It is released before it is deleted.
 */
static void destroy_registry_mutex_if_idle(void)
{
    if (s_registry_mutex == NULL) {
        return;
    }
    /* Look at the list while the lock is held. */
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    bool idle = (s_iface_list == NULL && !s_global_handlers_registered);
    xSemaphoreGive(s_registry_mutex);
    /* Delete only after the lock is released. Deleting a held lock is not allowed. */
    if (idle) {
        if (s_global_wait_events != NULL) {
            vEventGroupDelete(s_global_wait_events);
            s_global_wait_events = NULL;
        }
        vSemaphoreDelete(s_registry_mutex);
        s_registry_mutex = NULL;
    }
}

/*
 * Install the shared IP handler, and the Thread handler when Thread is enabled.
 *
 * The caller already holds the list lock, and must not hold an interface lock.
 * This function lets the list lock go while it talks to the event system, then
 * takes it again before returning. That avoids a deadlock: an event handler
 * runs while the event system holds its own lock, and the handler also needs
 * the list lock.
 *
 * "Already registered" is fine, because two creates can overlap. "No event
 * loop yet" is also fine: the flag stays clear and start() tries again later.
 * If a later handler cannot be installed, the handlers this call added are removed.
 */
static esp_err_t register_global_handlers(void)
{
    /*
     * Listen for our own events first, so the event-loop task is recorded
     * before an application callback runs. INVALID_ARG means this handler
     * is already installed. That is not a failure.
     */
    esp_err_t iface_err = esp_event_handler_register(NET_IFACE_EVENT, ESP_EVENT_ANY_ID,
                                                     &on_global_iface_event, NULL);
    if (iface_err != ESP_OK && iface_err != ESP_ERR_INVALID_ARG) {
        return iface_err;
    }
    /* Listen for every IP event: address gained and address lost. */
    esp_err_t err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_global_ip_event, NULL);
    if (err != ESP_OK && err != ESP_ERR_INVALID_ARG) {
        if (iface_err == ESP_OK) {
            esp_event_handler_unregister(NET_IFACE_EVENT, ESP_EVENT_ANY_ID, &on_global_iface_event);
        }
        return err;
    }
#if CONFIG_NET_CONNECT_THREAD
    esp_err_t ot_err = esp_event_handler_register(OPENTHREAD_EVENT, ESP_EVENT_ANY_ID,
                                                  &on_global_ot_event, NULL);
    if (ot_err != ESP_OK && ot_err != ESP_ERR_INVALID_ARG) {
        /* Do not leave a partial set of handlers installed. */
        if (err == ESP_OK) {
            esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_global_ip_event);
        }
        if (iface_err == ESP_OK) {
            esp_event_handler_unregister(NET_IFACE_EVENT, ESP_EVENT_ANY_ID, &on_global_iface_event);
        }
        return ot_err;
    }
#endif
    return ESP_OK;
}

static void unregister_global_handlers(void)
{
    /* Drop the shared listeners. Missing handlers are ignored by the event system. */
    esp_event_handler_unregister(NET_IFACE_EVENT, ESP_EVENT_ANY_ID, &on_global_iface_event);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_global_ip_event);
#if CONFIG_NET_CONNECT_THREAD
    esp_event_handler_unregister(OPENTHREAD_EVENT, ESP_EVENT_ANY_ID, &on_global_ot_event);
#endif
}

/*
 * Install the shared handlers if they are not installed yet.
 * Must be called without holding the list lock or any interface lock, because
 * registering a handler takes the event system's lock while an event callback
 * takes the list lock.
 * If the event loop does not exist yet, this does nothing and start() tries later.
 */
static void try_register_global_handlers(void)
{
    if (s_registry_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    bool already = s_global_handlers_registered;
    xSemaphoreGive(s_registry_mutex);
    if (already) {
        return;
    }

    esp_err_t err = register_global_handlers();
    if (err == ESP_OK) {
        xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
        s_global_handlers_registered = true;
        xSemaphoreGive(s_registry_mutex);
    }
}

/*
 * Remove the shared handlers when no interfaces are left. Otherwise the next
 * unit test deletes the event loop while a handler is still attached to it.
 * If another create added an interface while the lock was released, the
 * handlers are installed again for that interface.
 * Must be called without holding the list lock or any interface lock.
 */
static void maybe_unregister_global_handlers(void)
{
    if (s_registry_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    bool should_unregister = (s_iface_list == NULL && s_global_handlers_registered);
    xSemaphoreGive(s_registry_mutex);
    if (!should_unregister) {
        return;
    }

    unregister_global_handlers();

    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    s_global_handlers_registered = false;
    /* Forget the event-loop task. The next interface, IP, or Thread event records it again. */
    s_event_loop_task = NULL;
    bool list_non_empty = (s_iface_list != NULL);
    xSemaphoreGive(s_registry_mutex);

    /* A create may have added an interface while the handlers were removed. Put them back. */
    if (list_non_empty) {
        try_register_global_handlers();
    }
}

/* Free this interface: its wait flags, its lock, and the object itself. */
static void free_iface_obj(net_iface_handle_t h)
{
    if (h->state_events != NULL) {
        vEventGroupDelete(h->state_events);
        h->state_events = NULL;
    }
    if (h->lock != NULL) {
        vSemaphoreDelete(h->lock);
        h->lock = NULL;
    }
    free(h);
}

/* Remove this interface from the list. The caller holds the list lock. */
static void registry_remove_locked(net_iface_handle_t h)
{
    /* Walk the list by the link pointer, so the previous item can be updated in place. */
    net_iface_handle_t *link = &s_iface_list;
    while (*link != NULL) {
        if (*link == h) {
            *link = h->next;
            h->next = NULL;
            return;
        }
        link = &(*link)->next;
    }
}

/*
 * Tell listeners what just happened. The interface lock must already be
 * released. A listener might call net_iface_get_state(), which needs that lock.
 *
 * This does not wait. A full event queue does not stall the driver. If there
 * is no event loop yet, the notification is skipped. net_iface_wait() still
 * works, because it watches the flags rather than the event.
 * A listener that registers late does not receive events that already happened.
 */
static void post_iface_event(net_iface_handle_t h, net_iface_event_id_t event_id,
                             esp_netif_t *netif, net_iface_state_t state, esp_err_t err)
{
    /* Copy the facts into the event. Listeners must not use the handle's lock from here. */
    net_iface_event_data_t data = {
        .iface = h,
        .netif = netif,
        .type  = h->cfg.type,
        .state = state,
        .err   = err,
    };
    /* Timeout of 0 means "do not wait". A full queue just drops this notice. */
    esp_err_t post_ret = esp_event_post(NET_IFACE_EVENT, event_id, &data, sizeof(data), 0);
    /* No event loop yet is normal during early startup. Anything else is logged. */
    if (post_ret != ESP_OK && post_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to post NET_IFACE_EVENT %d: %s", (int)event_id, esp_err_to_name(post_ret));
    }
}

/*
 * The caller holds the interface lock.
 * Thread is ready once it has joined the network.
 * Wi-Fi, Ethernet, and PPP are ready when they have an IPv4 address, if IPv4
 * is turned on. If this interface also asked for IPv6, it must have the
 * preferred IPv6 address as well.
 */
static bool is_ready_satisfied_locked(net_iface_handle_t h)
{
    EventBits_t bits = xEventGroupGetBits(h->state_events);
    if (h->cfg.type == NET_IFACE_TYPE_THREAD) {
        /* Thread does not wait for an IP address. Joining the network is enough. */
        return (bits & NET_IFACE_BIT_OT_ATTACHED) != 0;
    }
#if CONFIG_NET_CONNECT_IPV4
    /* Still waiting for an IPv4 address. */
    if ((bits & NET_IFACE_BIT_IPV4) == 0) {
        return false;
    }
#endif
#if CONFIG_NET_CONNECT_IPV6
    /* IPv6 was requested for this interface, and the preferred address is not here yet. */
    if (h->cfg.ip.enable_ipv6 && (bits & NET_IFACE_BIT_IPV6) == 0) {
        return false;
    }
#endif
    return true;
}

/*
 * Find the interface that owns this network interface object.
 * The list lock is taken first, then each interface lock. deinit uses the
 * same order, so the two cannot deadlock.
 */
static net_iface_handle_t find_iface_by_netif(esp_netif_t *netif)
{
    if (netif == NULL || s_registry_mutex == NULL) {
        return NULL;
    }
    net_iface_handle_t found = NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    for (net_iface_handle_t cur = s_iface_list; cur != NULL; cur = cur->next) {
        /* Read netif under the interface lock. stop() can clear it at any time. */
        xSemaphoreTake(cur->lock, portMAX_DELAY);
        bool match = (cur->netif == netif);
        xSemaphoreGive(cur->lock);
        if (match) {
            found = cur;
            break;
        }
    }
    xSemaphoreGive(s_registry_mutex);
    return found;
}

/*
 * Set an address or attach flag on the interface and, if all required flags
 * are now present, mark it ready and tell listeners.
 * The check is done while holding the interface lock. The notification is
 * sent after the lock is released, so a listener can safely ask for the state.
 * An interface that is stopping, or that has not been started, is left alone.
 * A late IP event must not bring it back to life.
 */
static void set_readiness_bit(net_iface_handle_t h, EventBits_t bit)
{
    if (h == NULL) {
        return;
    }
    bool became_ready = false;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Skip a handle that is shutting down or was never started. */
    if (h->state != NET_IFACE_STATE_STOPPING && h->state != NET_IFACE_STATE_INIT &&
            h->state != NET_IFACE_STATE_UNINIT) {
        xEventGroupSetBits(h->state_events, bit);
        became_ready = is_ready_satisfied_locked(h);
    }
    xSemaphoreGive(h->lock);
    /* Notify only after the lock is released. The listener may read the state. */
    if (became_ready) {
        net_iface_notify_ready(h);
    }
}

/* The driver is up and its network interface exists. Ignored while stopping, or if start() was never called. */
void net_iface_notify_started(net_iface_handle_t h, esp_netif_t *netif)
{
    if (h == NULL) {
        return;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /*
     * A stop in progress must not look started, and an idle handle was never
     * started. Still remember the netif during stop, so the driver's stop
     * callback can free what start just created.
     */
    if (h->state == NET_IFACE_STATE_STOPPING || h->state == NET_IFACE_STATE_INIT) {
        if (h->state == NET_IFACE_STATE_STOPPING) {
            h->netif = netif;
        }
        xSemaphoreGive(h->lock);
        return;
    }
    /* Remember the network interface and leave the "starting" state. */
    h->netif = netif;
    h->state = NET_IFACE_STATE_STARTED;
    /* A previous stop flag would make a new wait return immediately. Clear it. */
    xEventGroupClearBits(h->state_events, NET_IFACE_BIT_STOPPED);
    xSemaphoreGive(h->lock);

    post_iface_event(h, NET_IFACE_EVENT_STARTED, netif, NET_IFACE_STATE_STARTED, ESP_OK);
}

/*
 * The link is up: Wi-Fi has associated, the cable is connected, and so on.
 * That is not the same as ready. The IP address may still be missing.
 * For IPv6, the link-local address is created after the lock is released.
 * Creating it can raise an IPv6 event, which comes back into the IP handler.
 */
void net_iface_notify_connected(net_iface_handle_t h)
{
    if (h == NULL) {
        return;
    }
    esp_netif_t *netif = NULL;
    bool enable_ipv6 = false;
    net_iface_type_t type = NET_IFACE_TYPE_MAX;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Ignore a second connect, a stop, or a handle that was never started. */
    if (h->state == NET_IFACE_STATE_STOPPING || h->state == NET_IFACE_STATE_INIT ||
            h->state == NET_IFACE_STATE_CONNECTED || h->state == NET_IFACE_STATE_READY) {
        xSemaphoreGive(h->lock);
        return;
    }
    h->state = NET_IFACE_STATE_CONNECTED;
    /* The link came up, so the retry counter starts over. */
    h->retry_num = 0;
    /* Copy what IPv6 setup needs, then drop the lock before calling the driver. */
    netif = h->netif;
    enable_ipv6 = h->cfg.ip.enable_ipv6;
    type = h->cfg.type;
    xSemaphoreGive(h->lock);

#if CONFIG_NET_CONNECT_IPV6
    /* Thread makes its own addresses. Everyone else gets a link-local address here. */
    if (netif != NULL && enable_ipv6 && type != NET_IFACE_TYPE_THREAD) {
        esp_err_t err = esp_netif_create_ip6_linklocal(netif);
        /* Some interfaces cannot make a link-local address. That case is not a failure here. */
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_INVALID_PARAMS) {
            const char *desc = esp_netif_get_desc(netif);
            ESP_LOGW(TAG, "Failed to create IPv6 link-local on %s: %s",
                     desc != NULL ? desc : "unknown", esp_err_to_name(err));
        }
    }
#else
    (void)enable_ipv6;
    (void)type;
#endif

    post_iface_event(h, NET_IFACE_EVENT_CONNECTED, netif, NET_IFACE_STATE_CONNECTED, ESP_OK);
}

/*
 * The interface has the address it needs. The ready flag stays set until the
 * IP is lost, the link drops, the interface stops, or a new attempt starts.
 * Because the flag stays set, net_iface_wait() still succeeds if it is called
 * after the interface is already ready.
 */
void net_iface_notify_ready(net_iface_handle_t h)
{
    if (h == NULL) {
        return;
    }
    esp_netif_t *netif = NULL;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Already ready, stopping, or never started. Do not post the event again. */
    if (h->state == NET_IFACE_STATE_STOPPING || h->state == NET_IFACE_STATE_INIT ||
            h->state == NET_IFACE_STATE_READY) {
        xSemaphoreGive(h->lock);
        return;
    }
    h->state = NET_IFACE_STATE_READY;
    h->last_err = ESP_OK;
    netif = h->netif;
    /* Drop any old failure, then raise the ready flag so waiters wake up. */
    xEventGroupClearBits(h->state_events, NET_IFACE_BIT_FAILED | NET_IFACE_BIT_STOPPED);
    xEventGroupSetBits(h->state_events, NET_IFACE_BIT_READY);
    xSemaphoreGive(h->lock);

    /* Wake group waits after the lock is released, so a higher-priority waiter can read the result. */
    pulse_global_wait_events();
    post_iface_event(h, NET_IFACE_EVENT_READY, netif, NET_IFACE_STATE_READY, ESP_OK);
}

/* The IPv4 address is gone. The interface is no longer ready and goes back to connecting. */
void net_iface_notify_lost_ip(net_iface_handle_t h)
{
    if (h == NULL) {
        return;
    }
    esp_netif_t *netif = NULL;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Only a live link can lose its address. Other states are left as they are. */
    if (h->state != NET_IFACE_STATE_READY && h->state != NET_IFACE_STATE_CONNECTED) {
        xSemaphoreGive(h->lock);
        return;
    }
    /* Back to connecting. The driver is still up and may get a new address. */
    h->state = NET_IFACE_STATE_CONNECTING;
    netif = h->netif;
    xEventGroupClearBits(h->state_events, NET_IFACE_BIT_READY | NET_IFACE_BIT_IPV4);
    xSemaphoreGive(h->lock);

    post_iface_event(h, NET_IFACE_EVENT_LOST_IP, netif, NET_IFACE_STATE_CONNECTING, ESP_OK);
}

/* The link dropped. The driver stays running, so connect() can be called again. */
void net_iface_notify_disconnected(net_iface_handle_t h)
{
    if (h == NULL) {
        return;
    }
    esp_netif_t *netif = NULL;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Already down, stopping, or never started. Nothing to disconnect. */
    if (h->state == NET_IFACE_STATE_STOPPING || h->state == NET_IFACE_STATE_INIT ||
            h->state == NET_IFACE_STATE_STARTED) {
        xSemaphoreGive(h->lock);
        return;
    }
    /* The driver is still running. Only the link and the address flags are gone. */
    h->state = NET_IFACE_STATE_STARTED;
    netif = h->netif;
    xEventGroupClearBits(h->state_events, NET_IFACE_BITS_LIVENESS);
    xSemaphoreGive(h->lock);

    post_iface_event(h, NET_IFACE_EVENT_DISCONNECTED, netif, NET_IFACE_STATE_STARTED, ESP_OK);
}

/*
 * Startup or connection failed. The error is saved, and the failed flag is set
 * so a wait returns that error right away. If the caller reports success as
 * the error, it is saved as a generic failure instead. A failed interface
 * must not look like it succeeded.
 */
void net_iface_notify_failed(net_iface_handle_t h, esp_err_t err)
{
    if (h == NULL) {
        return;
    }
    /* ESP_OK would make a waiter think the failure succeeded. Store a real error. */
    if (err == ESP_OK) {
        err = ESP_FAIL;
    }
    esp_netif_t *netif = NULL;
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* A second failure, a stop, or an idle handle does not overwrite the state. */
    if (h->state == NET_IFACE_STATE_STOPPING || h->state == NET_IFACE_STATE_INIT ||
            h->state == NET_IFACE_STATE_FAILED) {
        xSemaphoreGive(h->lock);
        return;
    }
    h->state = NET_IFACE_STATE_FAILED;
    h->last_err = err;
    netif = h->netif;
    /* Clear ready first, then raise failed, so a waiter cannot see both as the result. */
    xEventGroupClearBits(h->state_events, NET_IFACE_BIT_READY);
    xEventGroupSetBits(h->state_events, NET_IFACE_BIT_FAILED);
    xSemaphoreGive(h->lock);

    /* Wake group waits after the lock is released, so a higher-priority waiter can read the error. */
    pulse_global_wait_events();
    post_iface_event(h, NET_IFACE_EVENT_FAILED, netif, NET_IFACE_STATE_FAILED, err);
}

/*
 * The driver is stopped and its network interface is gone. The handle is back
 * to the initial state, so deinit() is allowed. Anyone still waiting is woken
 * and told the interface has stopped.
 */
void net_iface_notify_stopped(net_iface_handle_t h)
{
    if (h == NULL) {
        return;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* The network interface object is gone. Forget the pointer. */
    h->netif = NULL;
    h->state = NET_IFACE_STATE_INIT;
    /* Wipe ready, failed, and address flags, then wake waiters with "stopped". */
    xEventGroupClearBits(h->state_events, NET_IFACE_BITS_LIVENESS);
    xEventGroupSetBits(h->state_events, NET_IFACE_BIT_STOPPED);
    xSemaphoreGive(h->lock);

    /* Wake group waits after the lock is released. */
    pulse_global_wait_events();
    post_iface_event(h, NET_IFACE_EVENT_STOPPED, NULL, NET_IFACE_STATE_INIT, ESP_OK);
}

/*
 * Every interface shares this handler. The event says which network interface
 * got or lost an address, and that is matched to one of our handles.
 * Getting an address sets a flag. Losing an address drops the interface out
 * of the ready state. This also remembers which task is the event loop, so
 * wait() and stop() can refuse to run on it.
 */
static void on_global_ip_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    /* This task is the default event loop. Remember it for the wait and stop checks. */
    s_event_loop_task = xTaskGetCurrentTaskHandle();
    if (event_data == NULL) {
        return;
    }

    if (event_id == IP_EVENT_STA_GOT_IP ||
            event_id == IP_EVENT_ETH_GOT_IP ||
            event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        set_readiness_bit(find_iface_by_netif(event->esp_netif), NET_IFACE_BIT_IPV4);
    } else if (event_id == IP_EVENT_STA_LOST_IP ||
               event_id == IP_EVENT_ETH_LOST_IP ||
               event_id == IP_EVENT_PPP_LOST_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        net_iface_notify_lost_ip(find_iface_by_netif(event->esp_netif));
#if CONFIG_NET_CONNECT_IPV6
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        /* Only the preferred kind of IPv6 address counts. Other addresses are ignored. */
        if (esp_netif_ip6_get_addr_type(&event->ip6_info.ip) == NET_CONNECT_PREFERRED_IPV6_TYPE) {
            set_readiness_bit(find_iface_by_netif(event->esp_netif), NET_IFACE_BIT_IPV6);
        }
#endif
    }
}

#if CONFIG_NET_CONNECT_THREAD
/*
 * Thread is ready when it joins the network, not when it gets an IP address.
 * The event does not name a network interface, so the Thread handle is found
 * by type. Only one Thread interface is supported.
 */
static void on_global_ot_event(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;
    s_event_loop_task = xTaskGetCurrentTaskHandle();
    if (event_id == OPENTHREAD_EVENT_ATTACHED) {
        set_readiness_bit(net_iface_get_by_type(NET_IFACE_TYPE_THREAD), NET_IFACE_BIT_OT_ATTACHED);
    }
}
#endif

/*
 * Create a handle, save its settings, and add it to the list.
 * The driver's init function runs before the handle is on the list, and it
 * must not start the hardware. The shared IP handler is installed when the
 * first interface is created. If the event loop does not exist yet, start()
 * tries again later.
 */
esp_err_t net_iface_init_with_ops(const net_iface_config_t *cfg,
                                  const net_iface_ops_t *ops,
                                  net_iface_handle_t *out_handle)
{
    if (cfg == NULL || out_handle == NULL || ops == NULL || cfg->type >= NET_IFACE_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ensure_registry_mutex();
    if (err != ESP_OK) {
        return err;
    }

    /* One object holds the settings, the state, the lock, and the wait flags. */
    net_iface_handle_t h = calloc(1, sizeof(*h));
    if (h == NULL) {
        destroy_registry_mutex_if_idle();
        return ESP_ERR_NO_MEM;
    }
    h->lock = xSemaphoreCreateMutex();
    h->state_events = xEventGroupCreate();
    if (h->lock == NULL || h->state_events == NULL) {
        free_iface_obj(h);
        destroy_registry_mutex_if_idle();
        return ESP_ERR_NO_MEM;
    }

    h->cfg = *cfg;
    h->ops = ops;
    h->state = NET_IFACE_STATE_INIT;
    h->last_err = ESP_OK;

    /* Driver setup runs before the handle is visible to event handlers. */
    if (ops->init != NULL) {
        err = ops->init(h);
        if (err != ESP_OK) {
            free_iface_obj(h);
            destroy_registry_mutex_if_idle();
            return err;
        }
    }

    /* Put this handle at the front of the list, then install shared handlers if needed. */
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    h->next = s_iface_list;
    s_iface_list = h;
    xSemaphoreGive(s_registry_mutex);

    try_register_global_handlers();

    *out_handle = h;
    return ESP_OK;
}

/*
 * Create a handle and attach the driver that matches cfg->type.
 * The typed helpers (net_wifi_sta_create and the others) can also call
 * net_iface_init_with_ops() directly when they build the config themselves.
 */
esp_err_t net_iface_init(const net_iface_config_t *cfg, net_iface_handle_t *out_handle)
{
    if (cfg == NULL || out_handle == NULL || cfg->type >= NET_IFACE_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    const net_iface_ops_t *ops = NULL;
    switch (cfg->type) {
#if CONFIG_NET_CONNECT_WIFI
    case NET_IFACE_TYPE_WIFI_STA:
        ops = &g_net_iface_wifi_sta_ops;
        break;
#endif
#if CONFIG_NET_CONNECT_ETHERNET
    case NET_IFACE_TYPE_ETH:
        ops = &g_net_iface_eth_ops;
        break;
#endif
#if CONFIG_NET_CONNECT_THREAD
    case NET_IFACE_TYPE_THREAD:
        ops = &g_net_iface_thread_ops;
        break;
#endif
#if CONFIG_NET_CONNECT_PPP
    case NET_IFACE_TYPE_PPP:
        ops = &g_net_iface_ppp_ops;
        break;
#endif
    default:
        break;
    }
    /* No driver is compiled in for this type. */
    if (ops == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return net_iface_init_with_ops(cfg, ops, out_handle);
}

/*
 * The functions below exist so every function declared in net_iface.h can be
 * linked even when that interface is turned off in Kconfig. They do not touch
 * hardware. A real driver replaces them when the interface is enabled.
 */
#if !CONFIG_NET_CONNECT_WIFI || !CONFIG_NET_CONNECT_ETHERNET || \
    !CONFIG_NET_CONNECT_THREAD || !CONFIG_NET_CONNECT_PPP
static esp_err_t unsupported_create(net_iface_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif

#if !CONFIG_NET_CONNECT_WIFI
esp_err_t net_wifi_sta_create(const net_wifi_sta_params_t *params, net_iface_handle_t *out_handle)
{
    (void)params;
    return unsupported_create(out_handle);
}

esp_err_t net_wifi_sta_set_credentials(net_iface_handle_t h, const char *ssid, const char *password)
{
    (void)password;
    if (h == NULL || ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_ERR_NOT_SUPPORTED;
}
#endif

#if !CONFIG_NET_CONNECT_ETHERNET
esp_err_t net_eth_create(const net_eth_params_t *params, net_iface_handle_t *out_handle)
{
    (void)params;
    return unsupported_create(out_handle);
}
#endif

#if !CONFIG_NET_CONNECT_THREAD
esp_err_t net_thread_create(const net_thread_params_t *params, net_iface_handle_t *out_handle)
{
    (void)params;
    return unsupported_create(out_handle);
}
#endif

#if !CONFIG_NET_CONNECT_PPP
esp_err_t net_ppp_create(const net_ppp_params_t *params, net_iface_handle_t *out_handle)
{
    (void)params;
    return unsupported_create(out_handle);
}
#endif

/*
 * Free a handle that has been stopped. A running handle must be stopped first.
 * The list lock is taken before the interface lock. The interface lock is
 * released before the event handlers are removed and before the driver's
 * cleanup, because both of those can call back into this file.
 */
esp_err_t net_iface_deinit(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_registry_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* List lock first, then the interface lock. Same order as the lookups. */
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Only an idle handle can be destroyed. A running one must be stopped first. */
    if (h->state != NET_IFACE_STATE_INIT) {
        xSemaphoreGive(h->lock);
        xSemaphoreGive(s_registry_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    /* Take it off the list before anyone can look it up again. */
    registry_remove_locked(h);
    const net_iface_ops_t *ops = h->ops;
    /* Release both locks before handler removal and driver cleanup. */
    xSemaphoreGive(h->lock);
    xSemaphoreGive(s_registry_mutex);

    maybe_unregister_global_handlers();

    if (ops != NULL && ops->deinit != NULL) {
        ops->deinit(h);
    }
    h->state = NET_IFACE_STATE_UNINIT;
    free_iface_obj(h);
    /* Drop the list lock too, once nothing is left. */
    destroy_registry_mutex_if_idle();
    return ESP_OK;
}

/* Default priority for background interface startup tasks. */
#define NET_IFACE_STARTUP_TASK_PRIO 5

/* How long stop() waits for a startup task that is still running. */
#define NET_IFACE_STARTUP_STOP_WAIT_MS 10000

/*
 * Run one interface's driver start, and connect when the config asks for that.
 * The work runs here so a slow driver cannot block the caller or the other
 * interfaces. Ethernet with no cable can spend about 4 seconds in the PHY.
 */
static void iface_startup_task(void *arg)
{
    net_iface_handle_t h = (net_iface_handle_t)arg;

    esp_err_t err = h->ops->start(h);
    if (err != ESP_OK) {
        net_iface_notify_failed(h, err);
        goto done;
    }

    /* stop() may have arrived while the driver was starting. */
    xSemaphoreTake(h->lock, portMAX_DELAY);
    bool canceled = h->cancel_requested || (h->state == NET_IFACE_STATE_STOPPING);
    bool auto_connect = h->cfg.auto_connect_on_start;
    net_iface_state_t state = h->state;
    xSemaphoreGive(h->lock);

    if (!canceled && auto_connect && state == NET_IFACE_STATE_STARTED) {
        /*
         * connect() reports a real failure itself. An invalid-state result
         * (Wi-Fi with no network name, or a connect that already started)
         * leaves the interface started instead of failed.
         */
        (void)net_iface_connect(h);
    }

done:
    ESP_LOGD(TAG, "Startup task for '%s' finished (stack high water mark: %u bytes)",
             (h->ops && h->ops->name) ? h->ops->name : "iface",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));

    xSemaphoreTake(h->lock, portMAX_DELAY);
    h->startup_task = NULL;
    xEventGroupSetBits(h->state_events, NET_IFACE_BIT_STARTUP_DONE);
    xSemaphoreGive(h->lock);

    vTaskDelete(NULL);
}

/*
 * Begin startup from the initial state, or after a failure.
 * The driver start runs on a per-interface task. This returns as soon as that
 * task has been created. The state stays "starting" until the task reports
 * that the driver is up, or that startup failed.
 * Old ready, failed, and stopped flags are cleared first, so a wait cannot
 * return a result from the previous attempt.
 * The startup-done flag is cleared here and set again when that task exits.
 * It is not part of the liveness flags, because a disconnect must not forget
 * that the task is still running.
 */
esp_err_t net_iface_start(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Create may have run before the event loop existed. Try the handlers again. */
    try_register_global_handlers();

    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Start is allowed only from idle or from a previous failure, and when no task is still exiting. */
    if ((h->state != NET_IFACE_STATE_INIT && h->state != NET_IFACE_STATE_FAILED) ||
            h->startup_task != NULL) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (h->ops == NULL || h->ops->start == NULL) {
        /* notify_failed() ignores an idle handle, so leave "starting" first. */
        h->last_err = ESP_OK;
        h->cancel_requested = false;
        xEventGroupClearBits(h->state_events, NET_IFACE_BITS_LIVENESS | NET_IFACE_BIT_STARTUP_DONE);
        h->state = NET_IFACE_STATE_STARTING;
        xSemaphoreGive(h->lock);
        net_iface_notify_failed(h, ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }

    h->last_err = ESP_OK;
    h->cancel_requested = false;
    h->retry_num = 0;
    /* Forget the last attempt so a waiter blocks for this one. */
    xEventGroupClearBits(h->state_events, NET_IFACE_BITS_LIVENESS | NET_IFACE_BIT_STARTUP_DONE);
    h->state = NET_IFACE_STATE_STARTING;

    uint32_t stack_size = CONFIG_NET_CONNECT_STARTUP_TASK_STACK_SIZE;
    if (h->cfg.startup_task_stack_size > 0) {
        stack_size = (uint32_t)h->cfg.startup_task_stack_size;
    }
    const char *task_name = (h->ops->name != NULL) ? h->ops->name : "net_start";

    /*
     * Hold the lock across task creation. The new task is higher priority and
     * can run before xTaskCreate() returns. It must not clear startup_task
     * until that handle has been stored.
     */
    BaseType_t created = xTaskCreate(iface_startup_task, task_name, stack_size,
                                     h, NET_IFACE_STARTUP_TASK_PRIO, &h->startup_task);
    if (created != pdPASS) {
        h->startup_task = NULL;
        h->state = NET_IFACE_STATE_FAILED;
        h->last_err = ESP_ERR_NO_MEM;
        xEventGroupSetBits(h->state_events, NET_IFACE_BIT_FAILED | NET_IFACE_BIT_STARTUP_DONE);
        xSemaphoreGive(h->lock);
        pulse_global_wait_events();
        post_iface_event(h, NET_IFACE_EVENT_FAILED, NULL, NET_IFACE_STATE_FAILED, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(h->lock);
    return ESP_OK;
}

/*
 * Begin a new connection from "started" or "failed".
 * The old ready and failed flags are cleared, so a wait blocks for this
 * attempt instead of returning the previous error.
 * If the driver says the call is not valid right now (for example Wi-Fi has
 * no network name yet), the state goes back to started. Any other driver
 * error marks the interface failed.
 */
esp_err_t net_iface_connect(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    if (h->state != NET_IFACE_STATE_STARTED && h->state != NET_IFACE_STATE_FAILED) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    h->state = NET_IFACE_STATE_CONNECTING;
    h->last_err = ESP_OK;
    /* Clear the previous result. This attempt has not finished yet. */
    xEventGroupClearBits(h->state_events, NET_IFACE_BIT_READY | NET_IFACE_BIT_FAILED);
    xSemaphoreGive(h->lock);

    if (h->ops != NULL && h->ops->connect != NULL) {
        esp_err_t err = h->ops->connect(h);
        if (err != ESP_OK) {
            if (err == ESP_ERR_INVALID_STATE) {
                /* The driver was not ready to connect. Put the state back, do not mark failed. */
                xSemaphoreTake(h->lock, portMAX_DELAY);
                if (h->state == NET_IFACE_STATE_CONNECTING) {
                    h->state = NET_IFACE_STATE_STARTED;
                }
                xSemaphoreGive(h->lock);
            } else {
                net_iface_notify_failed(h, err);
            }
            return err;
        }
    }
    return ESP_OK;
}

/* Tell the driver to drop the link, then mark the interface started again. */
esp_err_t net_iface_disconnect(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Disconnect only makes sense while connecting, connected, ready, or failed. */
    if (h->state != NET_IFACE_STATE_CONNECTING &&
            h->state != NET_IFACE_STATE_CONNECTED &&
            h->state != NET_IFACE_STATE_READY &&
            h->state != NET_IFACE_STATE_FAILED) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    /* Drop the lock before the driver call. The driver may post events. */
    xSemaphoreGive(h->lock);

    esp_err_t err = ESP_OK;
    if (h->ops != NULL && h->ops->disconnect != NULL) {
        err = h->ops->disconnect(h);
    }
    /* The driver stays up. The link and address flags are cleared. */
    net_iface_notify_disconnected(h);
    return err;
}

/*
 * Shut the driver down and return the handle to the initial state.
 * This must not be called from the event-loop task. The driver's stop
 * function may need that task to finish an event, and calling it from the
 * task itself would wait forever.
 * If the interface is already stopped, this returns success.
 * If a startup task is still running, this waits for it to finish first.
 * The link is dropped before the driver is stopped, including when stop()
 * arrives while startup is still in progress and the link comes up afterward.
 * Even if the driver's stop function fails, the handle is marked stopped,
 * so it does not get stuck in "stopping".
 */
esp_err_t net_iface_stop(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Stopping from the event loop can wait for an event that never runs. */
    if (called_from_event_loop()) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* Already idle or stopping. Nothing to shut down. */
    if (h->state == NET_IFACE_STATE_INIT || h->state == NET_IFACE_STATE_STOPPING) {
        xSemaphoreGive(h->lock);
        return ESP_OK;
    }
    /*
     * A startup task may still be inside the driver. Wait until it leaves
     * before disconnect or stop, and before deinit() can free this handle.
     */
    bool wait_for_startup = (h->startup_task != NULL);
    h->cancel_requested = true;
    h->state = NET_IFACE_STATE_STOPPING;
    xSemaphoreGive(h->lock);

    if (wait_for_startup) {
        EventBits_t bits = xEventGroupWaitBits(h->state_events, NET_IFACE_BIT_STARTUP_DONE,
                                                pdFALSE, pdFALSE,
                                                pdMS_TO_TICKS(NET_IFACE_STARTUP_STOP_WAIT_MS));
        if ((bits & NET_IFACE_BIT_STARTUP_DONE) == 0) {
            ESP_LOGW(TAG, "Startup task did not finish within %d ms", NET_IFACE_STARTUP_STOP_WAIT_MS);
        }
    }

    /*
     * Disconnect whenever the driver has a disconnect callback. stop() can
     * arrive while the state is still "starting"; by the time the startup
     * task finishes, the link may already be up.
     */
    if (h->ops != NULL && h->ops->disconnect != NULL) {
        h->ops->disconnect(h);
    }

    esp_err_t err = ESP_OK;
    if (h->ops != NULL && h->ops->stop != NULL) {
        err = h->ops->stop(h);
    }
    /* Always leave the stopping state, even when the driver reports an error. */
    net_iface_notify_stopped(h);
    return err;
}

/*
 * Wait until the interface is ready, has failed, or has stopped.
 * The flags are not cleared by the wait. If the interface is already ready,
 * this returns immediately. Two tasks can wait on the same interface and both
 * wake up.
 * Ready is reported before failed, and failed before stopped.
 * The interface lock is not held during the wait.
 * If the time runs out, the interface keeps trying in the background.
 */
esp_err_t net_iface_wait(net_iface_handle_t h, uint32_t timeout_ms)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* A wait on the event loop would block the event that should wake it. */
    if (called_from_event_loop()) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(h->lock, portMAX_DELAY);
    /* There is nothing to wait for until start() has been called. */
    if (h->state == NET_IFACE_STATE_UNINIT || h->state == NET_IFACE_STATE_INIT) {
        xSemaphoreGive(h->lock);
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreGive(h->lock);

    /* UINT32_MAX means wait with no time limit. */
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    /* pdFALSE, pdFALSE: do not clear the flags, and wake on any one of them. */
    EventBits_t bits = xEventGroupWaitBits(h->state_events,
                                            NET_IFACE_BIT_READY | NET_IFACE_BIT_FAILED | NET_IFACE_BIT_STOPPED,
                                            pdFALSE, pdFALSE, ticks);
    if ((bits & NET_IFACE_BIT_READY) != 0) {
        return ESP_OK;
    }
    if ((bits & NET_IFACE_BIT_FAILED) != 0) {
        return net_iface_get_last_error(h);
    }
    if ((bits & NET_IFACE_BIT_STOPPED) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    /* None of the flags came up before the time ran out. The attempt continues. */
    return ESP_ERR_TIMEOUT;
}

/*
 * Check that the handle list is valid and that every interface has been
 * started. Used at the start of net_iface_wait_all() and net_iface_wait_any().
 * A stopped interface is allowed through: the scan below reports it.
 */
static esp_err_t validate_wait_handles(const net_iface_handle_t *handles, size_t count)
{
    if (handles == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < count; i++) {
        if (handles[i] == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (called_from_event_loop()) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < count; i++) {
        net_iface_handle_t h = handles[i];
        xSemaphoreTake(h->lock, portMAX_DELAY);
        net_iface_state_t state = h->state;
        EventBits_t bits = xEventGroupGetBits(h->state_events);
        xSemaphoreGive(h->lock);
        if (state == NET_IFACE_STATE_UNINIT ||
                (state == NET_IFACE_STATE_INIT && (bits & NET_IFACE_BIT_STOPPED) == 0)) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    return ESP_OK;
}

/* How long this shared wait may still sleep. portMAX_DELAY means no limit. */
static TickType_t wait_ticks_left(uint32_t timeout_ms, TickType_t start_ticks, TickType_t total_ticks)
{
    if (timeout_ms == UINT32_MAX) {
        return portMAX_DELAY;
    }
    TickType_t elapsed = xTaskGetTickCount() - start_ticks;
    if (elapsed >= total_ticks) {
        return 0;
    }
    return total_ticks - elapsed;
}

/*
 * Wait until every interface in the list is ready, within one shared timeout.
 * If any interface fails or stops, return that error right away.
 * The wake flag is cleared before each scan. A change that arrives during the
 * scan sets the flag again, so the sleep below returns immediately.
 */
esp_err_t net_iface_wait_all(const net_iface_handle_t *handles, size_t count, uint32_t timeout_ms)
{
    esp_err_t err = validate_wait_handles(handles, count);
    if (err != ESP_OK) {
        return err;
    }
    if (s_global_wait_events == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t total_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    while (true) {
        xEventGroupClearBits(s_global_wait_events, NET_IFACE_GLOBAL_WAKE_BIT);

        bool all_ready = true;
        for (size_t i = 0; i < count; i++) {
            net_iface_handle_t h = handles[i];
            EventBits_t bits = xEventGroupGetBits(h->state_events);

            if ((bits & NET_IFACE_BIT_FAILED) != 0) {
                esp_err_t last_err = net_iface_get_last_error(h);
                return (last_err != ESP_OK) ? last_err : ESP_FAIL;
            }
            if ((bits & NET_IFACE_BIT_STOPPED) != 0) {
                return ESP_ERR_INVALID_STATE;
            }
            if ((bits & NET_IFACE_BIT_READY) == 0) {
                net_iface_state_t state = net_iface_get_state(h);
                if (state == NET_IFACE_STATE_UNINIT || state == NET_IFACE_STATE_INIT) {
                    return ESP_ERR_INVALID_STATE;
                }
                all_ready = false;
            }
        }

        if (all_ready) {
            return ESP_OK;
        }

        TickType_t wait_ticks = wait_ticks_left(timeout_ms, start_ticks, total_ticks);
        if (wait_ticks == 0) {
            return ESP_ERR_TIMEOUT;
        }
        /* Do not clear the flag here. The next loop clears it before it scans. */
        (void)xEventGroupWaitBits(s_global_wait_events, NET_IFACE_GLOBAL_WAKE_BIT,
                                  pdFALSE, pdFALSE, wait_ticks);
    }
}

/*
 * Wait until at least one interface in the list is ready, within one shared
 * timeout. One failure does not end the wait while another interface is still
 * trying. When every interface has failed or stopped, return the first error.
 */
esp_err_t net_iface_wait_any(const net_iface_handle_t *handles, size_t count,
                             uint32_t timeout_ms, net_iface_handle_t *out_ready)
{
    if (out_ready != NULL) {
        *out_ready = NULL;
    }

    esp_err_t err = validate_wait_handles(handles, count);
    if (err != ESP_OK) {
        return err;
    }
    if (s_global_wait_events == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t total_ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    while (true) {
        xEventGroupClearBits(s_global_wait_events, NET_IFACE_GLOBAL_WAKE_BIT);

        size_t ended_count = 0;
        esp_err_t first_err = ESP_OK;

        for (size_t i = 0; i < count; i++) {
            net_iface_handle_t h = handles[i];
            EventBits_t bits = xEventGroupGetBits(h->state_events);

            if ((bits & NET_IFACE_BIT_READY) != 0) {
                if (out_ready != NULL) {
                    *out_ready = h;
                }
                return ESP_OK;
            }
            if ((bits & NET_IFACE_BIT_FAILED) != 0) {
                ended_count++;
                if (first_err == ESP_OK) {
                    esp_err_t last_err = net_iface_get_last_error(h);
                    first_err = (last_err != ESP_OK) ? last_err : ESP_FAIL;
                }
            } else if ((bits & NET_IFACE_BIT_STOPPED) != 0) {
                ended_count++;
            } else {
                net_iface_state_t state = net_iface_get_state(h);
                if (state == NET_IFACE_STATE_UNINIT || state == NET_IFACE_STATE_INIT) {
                    return ESP_ERR_INVALID_STATE;
                }
            }
        }

        if (ended_count == count) {
            return (first_err != ESP_OK) ? first_err : ESP_ERR_INVALID_STATE;
        }

        TickType_t wait_ticks = wait_ticks_left(timeout_ms, start_ticks, total_ticks);
        if (wait_ticks == 0) {
            return ESP_ERR_TIMEOUT;
        }
        (void)xEventGroupWaitBits(s_global_wait_events, NET_IFACE_GLOBAL_WAKE_BIT,
                                  pdFALSE, pdFALSE, wait_ticks);
    }
}

/* Return the network interface pointer. stop() may clear it, so it is copied while the lock is held. */
esp_netif_t *net_iface_get_netif(net_iface_handle_t h)
{
    if (h == NULL) {
        return NULL;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    esp_netif_t *netif = h->netif;
    xSemaphoreGive(h->lock);
    return netif;
}

/* Copy the state under the lock. Another task may be changing it. */
net_iface_state_t net_iface_get_state(net_iface_handle_t h)
{
    if (h == NULL) {
        return NET_IFACE_STATE_UNINIT;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    net_iface_state_t state = h->state;
    xSemaphoreGive(h->lock);
    return state;
}

/* The type is chosen at create time and never changes, so no lock is needed. */
net_iface_type_t net_iface_get_type(net_iface_handle_t h)
{
    if (h == NULL) {
        return NET_IFACE_TYPE_MAX;
    }
    return h->cfg.type;
}

/* Copy the last error under the lock. A failure can be recorded at any time. */
esp_err_t net_iface_get_last_error(net_iface_handle_t h)
{
    if (h == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(h->lock, portMAX_DELAY);
    esp_err_t err = h->last_err;
    xSemaphoreGive(h->lock);
    return err;
}

/* Return the first interface of this type, or NULL if there is none. */
net_iface_handle_t net_iface_get_by_type(net_iface_type_t type)
{
    if (s_registry_mutex == NULL) {
        return NULL;
    }
    net_iface_handle_t found = NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    for (net_iface_handle_t cur = s_iface_list; cur != NULL; cur = cur->next) {
        if (cur->cfg.type == type) {
            found = cur;
            break;
        }
    }
    xSemaphoreGive(s_registry_mutex);
    return found;
}

/* Find an interface by the description saved at create time. That string is not copied. */
net_iface_handle_t net_iface_get_by_desc(const char *desc)
{
    if (desc == NULL || s_registry_mutex == NULL) {
        return NULL;
    }
    net_iface_handle_t found = NULL;
    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    /* Compare the caller's string with the one stored in each interface. */
    for (net_iface_handle_t cur = s_iface_list; cur != NULL; cur = cur->next) {
        if (cur->cfg.desc != NULL && strcmp(cur->cfg.desc, desc) == 0) {
            found = cur;
            break;
        }
    }
    xSemaphoreGive(s_registry_mutex);
    return found;
}

/*
 * Call the callback once for each interface. The list is copied first, and
 * the list lock is released before the callback runs. If the lock stayed
 * held, a callback that looks up another interface would deadlock.
 * The callback returns false to stop early. It must not destroy an interface
 * that is still in the copied list.
 */
esp_err_t net_iface_for_each(net_iface_cb_t cb, void *ctx)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* No interfaces have been created. There is nothing to visit. */
    if (s_registry_mutex == NULL) {
        return ESP_OK;
    }

    xSemaphoreTake(s_registry_mutex, portMAX_DELAY);
    size_t count = 0;
    for (net_iface_handle_t cur = s_iface_list; cur != NULL; cur = cur->next) {
        count++;
    }
    net_iface_handle_t *snapshot = NULL;
    if (count > 0) {
        snapshot = calloc(count, sizeof(*snapshot));
        if (snapshot == NULL) {
            xSemaphoreGive(s_registry_mutex);
            return ESP_ERR_NO_MEM;
        }
        /* Copy the handles while the list cannot change. */
        size_t i = 0;
        for (net_iface_handle_t cur = s_iface_list; cur != NULL; cur = cur->next) {
            snapshot[i++] = cur;
        }
    }
    xSemaphoreGive(s_registry_mutex);

    /* Run the callback without the list lock. It may look up other interfaces. */
    for (size_t i = 0; i < count; i++) {
        if (!cb(snapshot[i], ctx)) {
            break;
        }
    }
    free(snapshot);
    return ESP_OK;
}

/*
 * Listen for interface events. Events that already happened are not sent
 * again. After registering, call net_iface_get_state() to see whether an
 * interface is already ready or has failed.
 */
esp_err_t net_iface_register_event_handler(esp_event_handler_t handler, void *arg)
{
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Any NET_IFACE_EVENT id is delivered to this handler. */
    return esp_event_handler_register(NET_IFACE_EVENT, ESP_EVENT_ANY_ID, handler, arg);
}

esp_err_t net_iface_unregister_event_handler(esp_event_handler_t handler)
{
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_event_handler_unregister(NET_IFACE_EVENT, ESP_EVENT_ANY_ID, handler);
}
