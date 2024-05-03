/* Copyright 2024 Tenera Care
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "lowpan6_ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>

//! How many ms to wait for GAP connection
#define BLE_CONNECT_TIMEOUT 10000

//! What port we'll listen on.
#define PORT 1234

static const char* TAG = "main";

static bool do_scan();
static int on_gap_event(struct ble_gap_event* event, void* arg);

static lowpan6_ble_driver_handle s_l6ble_handle;

static inline int on_gap_connected(struct ble_gap_event* event)
{
    if (event->connect.status == 0)
    {
        ESP_LOGI(TAG, "BLE GAP connection established");
    }
    else
    {
        ESP_LOGE(TAG, "BLE GAP connection failed; status=%d", event->connect.status);
        do_scan();
    }

    return 0;
}

static inline int on_gap_disconnected(struct ble_gap_event* event)
{
    ESP_LOGI(TAG, "BLE GAP connection disconnected; reason=%d", event->disconnect.reason);

    do_scan();

    return 0;
}

static void on_lowpan6_ble_event(
    lowpan6_ble_driver_handle handle,
    struct lowpan6_ble_event* event,
    void* userdata
)
{
    switch (event->type)
    {
    case LOWPAN6_BLE_EVENT_GAP_CONNECTED:
        on_gap_connected(event->gap_connected.event);
        break;
    case LOWPAN6_BLE_EVENT_GAP_DISCONNECTED:
        on_gap_disconnected(event->gap_connected.event);
        break;
    }
}

//! On discover, connect to _any_ device that advertises IPSS support.
static inline int on_gap_event_discovery(struct ble_gap_event* event)
{
    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to parse advertisement fields; rc=%d", rc);
        return 0;
    }

    if (lowpan6_ble_connectable(&event->disc))
    {
        // Cancel the scan so we can use the BLE device for connecting
        rc = ble_gap_disc_cancel();
        if (rc != 0 && rc != BLE_HS_EALREADY)
        {
            ESP_LOGE(TAG, "Failed to cancel scan; rc=%d", rc);
            return rc;
        }

        // Kick off a `lowpan6_ble` connection and register `on_lowpan6_ble_event` as a callback for
        // lowpan6 ble events. Note that the `lowpan6_ble` driver will replace the GAP event
        // callback in NimBLE here!
        esp_err_t err = lowpan6_ble_connect(
            s_l6ble_handle,
            &event->disc.addr,
            BLE_CONNECT_TIMEOUT,
            on_lowpan6_ble_event,
            NULL
        );
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to lowpan6_ble_connect; err=%d", err);
        }
    }

    return 0;
}

static int on_gap_event(struct ble_gap_event* event, void* arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        return on_gap_event_discovery(event);

    default:
        ESP_LOGD(TAG, "Ignoring BLE GAP event with type %d", event->type);
        break;
    }

    return 0;
}

static bool do_scan()
{
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to automatically infer address type; rc=%d", rc);
        return false;
    }

    struct ble_gap_disc_params disc_params;

    disc_params.filter_duplicates = 1;
    disc_params.passive           = 1;
    disc_params.itvl              = 0;
    disc_params.window            = 0;
    disc_params.filter_policy     = 0;
    disc_params.limited           = 0;

    // Start discovery and configure `on_gap_event` as our event callback. We'll use this to
    // initiate a lowpan6_ble connection once we've found the peer we want to connect to.
    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, on_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to start GAP discovery; rc=%d", rc);
        return false;
    }

    return true;
}

void on_sync()
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to ensure addr");
        return;
    }

    do_scan();
}

void on_reset(int reason)
{
    ESP_LOGI(TAG, "Resetting state; reason=%d", reason);
}

void nimble_host_task(void* param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void udp_task()
{
    // Accept incoming connections from any address on port PORT.
    struct sockaddr_in6 server_addr;
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr   = in6addr_any;
    server_addr.sin6_port   = htons(PORT);

    int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0)
    {
        ESP_LOGE(TAG, "failed to create socket; rc=%d", s);
        return;
    }

    if (bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        ESP_LOGE(TAG, "failed to bind address");
        close(s);
        return;
    }

    // We'll explicitly set 0 timeout here so we wait forever for incoming messages.
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        ESP_LOGE(TAG, "failed to clear socket recv timeout");
        return;
    }

    ESP_LOGI(TAG, "listening on port %d", PORT);

    // These buffers are hard-code sized to match the buffers in the corresponding client.
    uint8_t rx_buffer[128];
    uint8_t tx_buffer[192];  // A bit bigger to fit the extra characters we add to responses.
    while (1)
    {
        ESP_LOGI(TAG, "waiting to receive...");
        struct sockaddr_in6 recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);

        int len = recvfrom(
            s,
            rx_buffer,
            sizeof(rx_buffer),
            0,
            (struct sockaddr*)&recv_addr,
            &recv_addr_len
        );
        if (len < 0)
        {
            ESP_LOGE(TAG, "Failed to receive from socket; errno=%d", errno);
            break;
        }

        // NULL terminate whatever we received
        rx_buffer[len] = 0;

        char straddr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &recv_addr.sin6_addr, straddr, INET6_ADDRSTRLEN);
        ESP_LOGI(
            TAG,
            "Received %d bytes from addr=%s port=%u: %s",
            len,
            straddr,
            ntohs(recv_addr.sin6_port),
            rx_buffer
        );

        // Prepare our response (prepend their message with `echo: `
        snprintf((char*)tx_buffer, 192, "echo: %s", rx_buffer);

        // Respond!
        int rc = sendto(
            s,
            tx_buffer,
            strlen((char*)tx_buffer),
            0,
            (struct sockaddr*)&recv_addr,
            recv_addr_len
        );
        if (rc < 0)
        {
            ESP_LOGE(TAG, "Failed to send to socket; errno=%d", errno);
        }
    }
}

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize `lowpan6_ble` and the ESP-NETIF resources it requires.
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(lowpan6_ble_init());

    // Initialize NimBLE
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.reset_cb        = on_reset;
    ble_hs_cfg.sync_cb         = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    int rc = ble_svc_gap_device_name_set("l6ble-server");
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to set GAP device name; rc=%d", rc);
        return;
    }

    nimble_port_freertos_init(nimble_host_task);

    // Add a new lowpan6_ble netif.
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE();

    esp_netif_config_t cfg = {
        .base   = &base_cfg,
        .driver = NULL,
        .stack  = netstack_default_lowpan6_ble,
    };

    esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg);

    s_l6ble_handle = lowpan6_ble_create();
    if (s_l6ble_handle != NULL)
    {
        ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, s_l6ble_handle));
    }

    // Use this main thread to run our UDP task forever.
    udp_task();
}
