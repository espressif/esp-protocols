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
#include "host/util/util.h"
#include "lowpan6_ble.h"
#include "lwip/sockets.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>

static const char* TAG = "main";

static uint8_t own_addr_type;
struct sockaddr_in6 dest_addr;

#define PORT 1234

static void do_advertise();  // forward declaration

static int on_gap_event(struct ble_gap_event* event, void* arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        // A new connection was established or a connection attempt failed.
        ESP_LOGI(
            TAG,
            "connection %s; status=%d",
            event->connect.status == 0 ? "established" : "failed",
            event->connect.status
        );

        if (event->connect.status != 0)
        {
            // Connection failed; resume advertising.
            do_advertise();
        }
        else
        {
            // We've had a peer connect to us! We'll store their address in `dest_addr` so the
            // `udp_task` function below can send messages to the correct destination.
            struct ble_gap_conn_desc desc;
            int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            ip6_addr_t peer_ip_addr;
            ble_addr_to_link_local(&desc.peer_id_addr, &peer_ip_addr);

            dest_addr.sin6_addr.un.u32_addr[0] = peer_ip_addr.addr[0];
            dest_addr.sin6_addr.un.u32_addr[1] = peer_ip_addr.addr[1];
            dest_addr.sin6_addr.un.u32_addr[2] = peer_ip_addr.addr[2];
            dest_addr.sin6_addr.un.u32_addr[3] = peer_ip_addr.addr[3];
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        do_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d", event->adv_complete.reason);
        do_advertise();
        return 0;

    default:
        return 0;
    }

    return 0;
}

/** Start BLE advertisement.
 *
 * This is primarily pulled from ESP-IDF's NimBLE bleprph example:
 * -
 * https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/bleprph/main/bleprph.h
 *
 * The only change being we advertise support for IPSS.
 */
static void do_advertise()
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char* name;
    int rc;

    memset(&fields, 0, sizeof fields);

    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    name                         = ble_svc_gap_device_name();
    fields.name                  = (uint8_t*)name;
    fields.name_len              = strlen(name);
    fields.name_is_complete      = 1;

    // Advertise support for the Internet Protocol Support Services (IPSS).
    fields.uuids16             = (ble_uuid16_t[]) {BLE_UUID16_INIT(LOWPAN6_BLE_SERVICE_UUID_IPSS)};
    fields.num_uuids16         = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, on_gap_event, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE state reset; reason=%d", reason);
}

static void on_sync()
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to determine address type; rc=%d", rc);
        return;
    }

    do_advertise();
}

void nimble_task(void* params)
{
    ESP_LOGI(TAG, "BLE host task started");

    nimble_port_run();
    nimble_port_freertos_deinit();
}

void udp_task(esp_netif_t* lowpan6_netif)
{
    char rx_buffer[128];

    // Wait for the netif to come up. This is a pretty graceless way to do this -- the better way
    // would be to add a `lowpan6_ble` event for "netif up" that we can subscribe to in our
    // `lowpan6_ble_create_server` callback. Not currently implemented though so for the sake of
    // this example...
    while (!esp_netif_is_netif_up(lowpan6_netif))
    {
        ESP_LOGI(TAG, "netif not up, waiting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Set our destination address family, port, and scope ID. We'll set the `dest_addr.sin6_addr`
    // in our GAP event callback: our peer's sin6_addr will be calculated from its BLE address.
    //
    // Note that it is important to set the `sin6_scope_id` here!!! It will be used by lwIP in
    // `sendto` to determine which netif we should use to send this UDP packet.
    dest_addr.sin6_family   = AF_INET6;
    dest_addr.sin6_port     = htons(PORT);
    dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(lowpan6_netif);

    int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket; errno=%d", errno);
        return;
    }

    // We'll set a socket timeout for receives here so we continue trying to send messages even if
    // the peer gets disconnected or something.
    struct timeval tv;
    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        ESP_LOGE(TAG, "Failed to set socket timeout");
        return;
    }

    const char* payload = "hello it's me!!!";
    while (1)
    {
        char addr_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &dest_addr.sin6_addr, addr_str, INET6_ADDRSTRLEN);
        ESP_LOGI(TAG, "sending to %s", addr_str);

        // Fire a message over to the dest_addr. Note that the `sin6_addr` for this gets set in our
        // GAP connect callback.
        int err = sendto(
            sock,
            payload,
            strlen(payload),
            0,
            (struct sockaddr*)&dest_addr,
            sizeof(dest_addr)
        );
        if (err < 0)
        {
            ESP_LOGE(TAG, "Failed to send payload; errno=%d", errno);
            break;
        }

        // Wait for a response from the echo server.
        struct sockaddr_in6 recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        int len                 = recvfrom(
            sock,
            rx_buffer,
            sizeof(rx_buffer) - 1,
            0,
            (struct sockaddr*)&recv_addr,
            &recv_addr_len
        );
        if (len < 0)
        {
            if (errno == EAGAIN)
            {
                ESP_LOGD(TAG, "Receive timed out");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to receive from socket; errno=%d", errno);
                break;
            }
        }
        else
        {
            rx_buffer[len] = 0;  // null terminate whatever we got
            ESP_LOGI(TAG, "Received %d bytes: `%s`", len, rx_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (sock != -1)
    {
        shutdown(sock, 0);
        close(sock);
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

    int rc = ble_svc_gap_device_name_set("l6ble-client");
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to set GAP device name; rc=%d", rc);
        return;
    }

    nimble_port_freertos_init(nimble_task);

    // Add a new lowpan6_ble netif.
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE();

    esp_netif_config_t cfg = {
        .base   = &base,
        .driver = NULL,
        .stack  = netstack_default_lowpan6_ble,
    };

    esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg);

    lowpan6_ble_driver_handle lowpan6_ble_driver = lowpan6_ble_create();
    if (lowpan6_ble_driver != NULL)
    {
        ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, lowpan6_ble_driver));
    }

    // Finally, register our `lowpan6_ble_driver` as an L2CAP server. This will hook L2CAP events
    // into the required driver events.
    lowpan6_ble_create_server(lowpan6_ble_driver, NULL, NULL);

    // Use this main thread to run our UDP task forever.
    udp_task(lowpan6_ble_netif);
}
