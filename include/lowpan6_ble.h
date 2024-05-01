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

#pragma once

#include "esp_compiler.h"
#include "esp_idf_version.h"
#include "host/ble_gap.h"
#include "host/ble_l2cap.h"
#include "lwip/ip6_addr.h"
#include "nimble/ble.h"

#include <stdbool.h>

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)
    // Add missing includes to esp_netif_types.h
    // https://github.com/espressif/esp-idf/commit/822129e234aaedf86e76fe92ab9b49c5f0a612e0
    #include "esp_err.h"
    #include "esp_event_base.h"
    #include "esp_netif_ip_addr.h"

    #include <stdbool.h>
    #include <stdint.h>
#endif

#include "esp_netif_types.h"

/** Maximum concurrent IPSP channels */
#define LOWPAN6_BLE_IPSP_MAX_CHANNELS 1

/** Maximum Transmit Unit on an IPSP channel.
 *
 * This is required by the specification to be 1280 (it's the minimum MTU for
 * IPv6).
 */
#define LOWPAN6_BLE_IPSP_MTU 1280

/** Maximum data size that can be received.
 *
 * This value can be modified to be lower than the MTU set on the channel.
 */
#define LOWPAN6_BLE_IPSP_RX_BUFFER_SIZE 1280

/** Maximum number of receive buffers.
 *
 * Each receive buffer is of size LOWPAN6_BLE_IPSP_RX_BUFFER_SIZE. Tweak this
 * value to modify the number of Service Data Units (SDUs) that can be received
 * while an SDU is being consumed by the application.
 */
#define LOWPAN6_BLE_IPSP_RX_BUFFER_COUNT 4

/** The IPSP L2CAP Protocol Service Multiplexer number.
 *
 * Defined by the Bluetooth Low Energy specification. See:
 *    https://www.bluetooth.com/specifications/assigned-numbers/
 */
#define LOWPAN6_BLE_IPSP_PSM 0x0023

/** The BLE Service UUID for the Internet Protocol Support Service
 *
 * Defined by the Bluetooth Low Energy specification. See:
 *    https://www.bluetooth.com/specifications/assigned-numbers/
 */
#define LOWPAN6_BLE_SERVICE_UUID_IPSS 0x1820

// clang-format off
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE() {              \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)     \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
     .get_ip_event  = 0,                                        \
     .lost_ip_event = 0,                                        \
     .if_key        = "LOWPAN6_BLE_DEF",                        \
     .if_desc       = "lowpan6_ble",                            \
     .route_prio    = 16,                                       \
     .bridge_info   = NULL                                      \
}
#else
#define ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE() {              \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(mac)     \
     ESP_COMPILER_DESIGNATED_INIT_AGGREGATE_TYPE_EMPTY(ip_info) \
     .get_ip_event  = 0,                                        \
     .lost_ip_event = 0,                                        \
     .if_key        = "LOWPAN6_BLE_DEF",                        \
     .if_desc       = "lowpan6_ble",                            \
     .route_prio    = 16,                                       \
}
#endif
// clang-format on

enum lowpan6_ble_event_type
{
    LOWPAN6_BLE_EVENT_GAP_CONNECTED,
    LOWPAN6_BLE_EVENT_GAP_DISCONNECTED
};

//! Event struct for LoWPAN6 BLE events.
struct lowpan6_ble_event
{
    //! Discriminator for the event data included in this event.
    enum lowpan6_ble_event_type type;

    union
    {
        //! Data available for type LOWPAN6_BLE_EVENT_GAP_CONNECTED.
        struct
        {
            //! The underlying GAP event.
            struct ble_gap_event* event;
        } gap_connected;

        //! Data available for type LOWPAN6_BLE_EVENT_GAP_DISCONNECTED.
        struct
        {
            //! The underlying GAP event.
            struct ble_gap_event* event;
        } gap_disconnected;
    };
};

extern esp_netif_netstack_config_t* netstack_default_lowpan6_ble;

typedef struct lowpan6_ble_driver* lowpan6_ble_driver_handle;

/** A LoWPAN6 BLE event handler
 *
 * @param[in] handle Handle to the lowpan6_ble driver processing this event.
 * @param[in] event Pointer to a lowpan6_ble_event struct.
 * @param[in] userdata (Optional) Arbitrary data provided by the user during callback registration.
 */
typedef void (*lowpan6_ble_event_handler)(
    lowpan6_ble_driver_handle handle,
    struct lowpan6_ble_event* event,
    void* userdata
);

/** Initialize the LoWPAN6 BLE module.
 *
 * This must be called once before creating lowpan6_ble drivers.
 *
 * @returns ESP_OK on success
 */
esp_err_t lowpan6_ble_init();

/** Create a new lowpan6_ble driver instance.
 *
 * @example
 * ```c
 * void app_main()
 * {
 *     esp_netif_config_t cfg = {
 *         .base = ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE(),
 *         .driver = NULL,
 *         .stack = netstack_default_lowpan6_ble,
 *     };
 *
 *     esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg);
 *
 *     lowpan6_ble_driver_handle lowpan6_ble_driver = lowpan6_ble_create();
 *     if (lowpan6_ble_driver != NULL)
 *     {
 *         ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, lowpan6_ble_driver));
 *     }
 * }
 * ```
 *
 * @returns A lowpan6_ble_driver_handle on success, NULL otherwise.
 */
lowpan6_ble_driver_handle lowpan6_ble_create();

/** Destroy the given lowpan6_ble_driver, freeing its resources.
 *
 * @param[in] driver The driver to destroy
 *
 * @returns ESP_OK on success
 */
esp_err_t lowpan6_ble_destroy(lowpan6_ble_driver_handle driver);

/** Determine if the advertising device can be connected over LoWPAN6 BLE
 *
 * This verifies that the device:
 *   - is BLE connectable
 *   - advertises support for the necessary Internet Protocol Support Service
 *
 * @example
 * ```c
 * static int my_gap_event_handler(struct ble_gap_event* event, void* arg)
 * {
 *     switch (event->type)
 *     {
 *     case BLE_GAP_EVENT_DISC:
 *         if (l6ble_io_connectable(event->disc))
 *         {
 *             ESP_LOGD(TAG, "can connect with lowpan6");
 *         }
 *         else
 *         {
 *             ESP_LOGD(TAG, "can't connect with lowpan6");
 *         }
 *     default:
 *         // ignored
 *         break;
 *     }
 * }
 * ```
 *
 * @param[in] disc The BLE GAP discovery descriptor. Typically obtained from a BLE_GAP_EVENT_DISC
 *    callback.
 *
 * @returns True if the device is connectable, False otherwise.
 */
bool lowpan6_ble_connectable(struct ble_gap_disc_desc* disc);

/** Establish a LoWPAN6 BLE connection with the given BLE address.
 *
 * This creates a GAP connection and attaches a callback that will ultimately establish an IPSP
 * connection with the device.
 *
 * @note The user should cancel discover before calling this function.
 *
 * @note This function will REPLACE any existing GAP event callback (i.e., the callback you set to
 * process the BLE_GAP_EVENT_DISC event).
 *
 * @example
 * ```
 * static int my_gap_event_handler(struct ble_gap_event* event, void* arg)
 * {
 *     switch (event->type)
 *     {
 *     case BLE_GAP_EVENT_DISC:
 *         if (lowpan6_ble_connectable(event->disc))
 *         {
 *             rc = ble_gap_disc_cancel();
 *             if (rc != 0 && rc != BLE_HS_EALREADY)
 *             {
 *                 ESP_LOGE(TAG, "Failed to cancel scan; rc=%d", rc);
 *                 return rc;
 *             }
 *
 *             lowpan6_ble_connect(l6ble_handle, &event->disc.addr, 10000, NULL, NULL);
 *         }
 *     default:
 *         // ignored
 *         break;
 *     }
 * }
 * ```
 *
 * @param[in] handle The lowpan6_ble instance to associate with this BLE connection.
 * @param[in] addr The BLE address to connect to.
 * @param[in] timeout_ms The maximum timeout for connecting, in milliseconds.
 * @param[in] cb (Optional) An event handler for lowpan6_ble events.
 * @param[in] userdata (Optional) Arbitrary data to pass to `cb` during LoWPAN6 BLE events.
 *
 * @returns ESP_OK if the connection was initiated successfully. Other codes
 *    indicate error. Note that success here simply means that the GAP connection
 *    was initiated. Further connection failures/successes are communicated via
 *    BLE callback events.
 */
esp_err_t lowpan6_ble_connect(
    lowpan6_ble_driver_handle handle,
    ble_addr_t* addr,
    int32_t timeout_ms,
    lowpan6_ble_event_handler cb,
    void* userdata
);

/** A NimBLE ble_l2cap_event_fn used to create an L2CAP server for LoWPAN6 BLE connections.
 *
 * This should be used to register an L2CAP server on LoWPAN6 BLE nodes (i.e., the devices
 * _accepting_ a connection request from the central device).
 *
 * @example
 * ```
 * void app_main()
 * {
 *     esp_netif_config_t cfg = {
 *         .base = ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE(),
 *         .driver = NULL,
 *         .stack = netstack_default_lowpan6_ble,
 *     };
 *
 *     esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg);
 *
 *     lowpan6_ble_driver_handle lowpan6_ble_driver = lowpan6_ble_create();
 *     if (lowpan6_ble_driver != NULL)
 *     {
 *         ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, lowpan6_ble_driver));
 *     }
 *
 *     // this will hook up our driver to our L2CAP channel once the other
 *     // end initiates the connection
 *     lowpan6_ble_create_server(lowpan6_ble_driver, NULL, NULL);
 * }
 * ```
 *
 * @param[in] handle The lowpan6_ble instance to associate with this BLE connection.
 * @param[in] cb (Optional) An event handler for lowpan6_ble events.
 * @param[in] userdata (Optional) Arbitrary data to pass to `cb` during LoWPAN6 BLE events.
 *
 * @returns ESP_OK if the connection was initiated successfully. Other codes
 *    indicate error. Note that success here simply means that the GAP connection
 *    was initiated. Further connection failures/successes are communicated via
 *    BLE callback events.
 */
esp_err_t lowpan6_ble_create_server(
    lowpan6_ble_driver_handle handle,
    lowpan6_ble_event_handler cb,
    void* userdata
);

/** Transform the given BLE address into a link-local IPv6 address.
 *
 * @param[in] ble_addr The BLE address to transform.
 * @param[out] ip_addr The output IPv6 address.
 *
 * @returns ESP_OK on success.
 */
esp_err_t ble_addr_to_link_local(ble_addr_t* ble_addr, ip6_addr_t* ip_addr);
