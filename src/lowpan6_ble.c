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

#include "lowpan6_ble.h"

#include "debug_print_utils.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "lowpan6_ble_netif.h"

/** The chunk of defines below are used to initialize mbufs later in this module.
 *
 * The MyNewt documentation explains what these are:
 *  https://mynewt.apache.org/latest/os/core_os/mbuf/mbuf.html
 */
#define MBUF_PKTHDR_OVERHEAD   sizeof(struct os_mbuf_pkthdr)
#define MBUF_MEMBLOCK_OVERHEAD sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD
#define MBUF_NUM_MBUFS         (LOWPAN6_BLE_IPSP_MAX_CHANNELS * LOWPAN6_BLE_IPSP_RX_BUFFER_COUNT)
#define MBUF_PAYLOAD_SIZE      LOWPAN6_BLE_IPSP_RX_BUFFER_SIZE
#define MBUF_BUF_SIZE          OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE     (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE      OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static const char* TAG = "lowpan6_ble";

//! Pool of mbufs, shared by all the channels in this module.
static struct os_mbuf_pool s_mbuf_pool;

//! Memory pool used to initialize our mbuf pool, above.
static struct os_mempool s_mempool;

//! Memory allocated for our memory pool, above.
static os_membuf_t s_membuf[MBUF_MEMPOOL_SIZE];

#define BIT_TX_UNSTALLED (1 << 0)
static StaticEventGroup_t s_lowpan6_event_group_buffer;
static EventGroupHandle_t s_lowpan6_event_group;

/** LoWPAN6 BLE driver
 *
 * This struct provides glue logic between esp_netif and the BLE channel used as a transport.
 */
struct lowpan6_ble_driver
{
    // esp_netif driver base
    esp_netif_driver_base_t base;

    // Pointer to L2CAP channel used for LoWPAN6-BLE
    struct ble_l2cap_chan* chan;

    // (Optional) event handler provided by the user.
    lowpan6_ble_event_handler cb;

    // (Optional) event handler data provided by the user.
    void* userdata;
};

void user_notify(struct lowpan6_ble_driver* driver, struct lowpan6_ble_event* event)
{
    if (driver && driver->cb && event)
    {
        driver->cb(driver, event, driver->userdata);
    }
}

static inline int set_recv_ready(struct ble_l2cap_chan* chan)
{
    struct os_mbuf* next = os_mbuf_get_pkthdr(&s_mbuf_pool, 0);

    int rc = ble_l2cap_recv_ready(chan, next);
    return rc;
}

static int on_l2cap_event(struct ble_l2cap_event* event, void* arg)
{
    ESP_LOGD(TAG, "(%s) event->type=%d", __func__, event->type);

    struct lowpan6_ble_driver* driver = (struct lowpan6_ble_driver*)arg;

    int rc = 0;
    switch (event->type)
    {
    case BLE_L2CAP_EVENT_COC_CONNECTED:
        driver->chan = event->connect.chan;
        struct ble_gap_conn_desc desc;
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        if (rc != 0)
        {
            ESP_LOGE(
                TAG,
                "(%s) could not find GAP conn with handle %d",
                __func__,
                event->connect.conn_handle
            );
            return rc;
        }

        lowpan6_ble_netif_up(driver->base.netif, &desc.peer_id_addr, &desc.our_id_addr);
        break;

    case BLE_L2CAP_EVENT_COC_DISCONNECTED:
        driver->chan = NULL;
        lowpan6_ble_netif_down(driver->base.netif);
        break;

    case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
        esp_netif_receive(
            driver->base.netif,
            event->receive.sdu_rx->om_data,
            event->receive.sdu_rx->om_len,
            event->receive.sdu_rx
        );

        // On data received, we need to provide NimBLE with the next sdu_rx to receive data into.
        // We'll do this after the user callback automatically.
        os_mbuf_free_chain(event->receive.sdu_rx);
        rc = set_recv_ready(event->receive.chan);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "(%s) couldn't set up next recv ready; rc=%d", __func__, rc);
        }

        break;

    case BLE_L2CAP_EVENT_COC_ACCEPT:
        rc = set_recv_ready(event->accept.chan);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "(%s) couldn't set up next recv ready; rc=%d", __func__, rc);
        }
        break;

    case BLE_L2CAP_EVENT_COC_TX_UNSTALLED:
        xEventGroupSetBits(s_lowpan6_event_group, BIT_TX_UNSTALLED);
        ESP_LOGD(
            TAG,
            "(%s) tx_unstalled; chan=%p status=%d",
            __func__,
            event->tx_unstalled.chan,
            event->tx_unstalled.status
        );
        break;

    default:
        ESP_LOGD(TAG, "(%s) ignoring BLE L2CAP event with type %d", __func__, event->type);
        break;
    }

    return rc;
}

static inline int
on_gap_event_connect(struct lowpan6_ble_driver* driver, struct ble_gap_event* event)
{
    if (event->connect.status != 0)
    {
        ESP_LOGE(TAG, "(%s) connection failed; status=%d", __func__, event->connect.status);
        return ESP_FAIL;
    }

    struct ble_gap_conn_desc desc;
    int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
    if (rc != 0)
    {
        ESP_LOGE(
            TAG,
            "(%s) connection not found; conn_handle=%d",
            __func__,
            event->connect.conn_handle
        );
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGD(
        TAG,
        "(%s) connection established; conn_handle=%d",
        __func__,
        event->connect.conn_handle
    );

    struct os_mbuf* sdu_rx = os_mbuf_get_pkthdr(&s_mbuf_pool, 0);
    if (sdu_rx == NULL)
    {
        ESP_LOGE(TAG, "(%s) cannot allocate memory for connect mbuf", __func__);
        return ESP_ERR_NO_MEM;
    }

    rc = ble_l2cap_connect(
        event->connect.conn_handle,
        LOWPAN6_BLE_IPSP_PSM,
        LOWPAN6_BLE_IPSP_MTU,
        sdu_rx,
        on_l2cap_event,
        driver
    );

    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to l2cap connect; rc=%d", __func__, rc);
        return rc;
    }

    return ESP_OK;
}

static inline int
on_gap_event_disconnect(struct lowpan6_ble_driver* driver, struct ble_gap_event* event)
{
    ESP_LOGD(TAG, "(%s) disconnected; reason=%d", __func__, event->disconnect.reason);

    return 0;
}

static int on_gap_event(struct ble_gap_event* event, void* arg)
{
    struct lowpan6_ble_driver* driver = (struct lowpan6_ble_driver*)arg;

    ESP_LOGD(TAG, "(%s) GAP event; event->type=%d", __func__, event->type);

    int rc = 0;
    struct lowpan6_ble_event out_event;
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        rc                            = on_gap_event_connect(driver, event);
        out_event.type                = LOWPAN6_BLE_EVENT_GAP_CONNECTED;
        out_event.gap_connected.event = event;
        user_notify(driver, &out_event);
        return rc;

    case BLE_GAP_EVENT_DISCONNECT:
        rc                               = on_gap_event_disconnect(driver, event);
        out_event.type                   = LOWPAN6_BLE_EVENT_GAP_DISCONNECTED;
        out_event.gap_disconnected.event = event;
        user_notify(driver, &out_event);
        return rc;

    default:
        ESP_LOGD(TAG, "(%s) ignoring BLE GAP event with type %d", __func__, event->type);
        break;
    }

    return rc;
}

static void lowpan6_ble_free_rx_buffer(void* h, void* buffer)
{
    int rc = os_mbuf_free_chain(buffer);
    if (rc != 0)
    {
        ESP_LOGW(TAG, "(%s) failed to free os_mbuf; om=%p rc=%d", __func__, buffer, rc);
    }
}

static esp_err_t lowpan6_ble_transmit(void* h, void* buffer, size_t len)
{
    struct lowpan6_ble_driver* driver = (struct lowpan6_ble_driver*)h;
    if (driver == NULL || driver->chan == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf* sdu_tx = os_mbuf_get_pkthdr(&s_mbuf_pool, 0);
    if (sdu_tx == NULL)
    {
        ESP_LOGE(TAG, "(%s) cannot allocate memory for output mbuf", __func__);
        return ESP_ERR_NO_MEM;
    }

    int rc = os_mbuf_append(sdu_tx, buffer, len);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) could not append data to mbuf; rc=%d", __func__, rc);
        os_mbuf_free_chain(sdu_tx);
        return ESP_FAIL;
    }

    // We'll send data over L2CAP. If the transfer gets stalled (receiving side needs to send us
    // credits before we continue), NimBLE will handle sending the rest of our message on receipt of
    // those credits (great). This means we _don't_ need to retry ble_l2cap_send on BLE_HS_ESTALLED.
    // If, however, there's already a stalled transmission for some OTHER message, we'll get
    // BLE_HS_EBUSY. In _this_ case, we'll wait for the unstalled event and try again in case the
    // previous operation completed.
    do
    {
        ESP_LOGD(TAG, "(%s) sending; sdu_tx=%p", __func__, sdu_tx);
        rc = ble_l2cap_send(driver->chan, sdu_tx);
        if (rc == BLE_HS_EBUSY)
        {
            ESP_LOGD(TAG, "(%s) waiting for unstall; sdu_tx=%p", __func__, sdu_tx);
            xEventGroupWaitBits(
                s_lowpan6_event_group,
                BIT_TX_UNSTALLED,
                pdTRUE,
                pdTRUE,
                portMAX_DELAY
            );
        }
        else if (rc != 0 && rc != BLE_HS_ESTALLED)
        {
            ESP_LOGW(TAG, "(%s) failed to send data via ipsp; rc=%d", __func__, rc);
            return ESP_FAIL;
        }
    } while (rc == BLE_HS_EBUSY);

    return ESP_OK;
}

static esp_err_t lowpan6_ble_post_attach(esp_netif_t* esp_netif, void* args)
{
    struct lowpan6_ble_driver* driver = (struct lowpan6_ble_driver*)args;

    ESP_LOGD(TAG, "(%s) esp_netif=%p args=%p", __func__, esp_netif, args);

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = lowpan6_ble_free_rx_buffer,
        .transmit              = lowpan6_ble_transmit,
        .handle                = driver
    };

    driver->base.netif = esp_netif;
    esp_err_t err      = esp_netif_set_driver_config(esp_netif, &driver_ifconfig);

    esp_netif_action_start(driver->base.netif, 0, 0, NULL);

    return err;
}

esp_err_t lowpan6_ble_init()
{
    int rc =
        os_mempool_init(&s_mempool, MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE, s_membuf, "lowpan6_ble");
    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to initialize mempool; rc=%d", __func__, rc);
        return ESP_FAIL;
    }

    rc = os_mbuf_pool_init(&s_mbuf_pool, &s_mempool, MBUF_MEMBLOCK_SIZE, MBUF_NUM_MBUFS);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to initialize mbuf pool; rc=%d", __func__, rc);
        os_mempool_clear(&s_mempool);
        return ESP_FAIL;
    }

    s_lowpan6_event_group = xEventGroupCreateStatic(&s_lowpan6_event_group_buffer);

    // we should _never_ hit this since we're initializing from a static event group. Still, for
    // completeness' sake...
    if (s_lowpan6_event_group == NULL)
    {
        ESP_LOGE(TAG, "(%s) failed to initialize event group", __func__);
        return ESP_FAIL;
    }

    return ESP_OK;
}

lowpan6_ble_driver_handle lowpan6_ble_create()
{
    ESP_LOGI(TAG, "(%s) creating lowpan6_ble driver", __func__);

    struct lowpan6_ble_driver* driver = calloc(1, sizeof(struct lowpan6_ble_driver));
    if (driver == NULL)
    {
        ESP_LOGE(TAG, "(%s) failed to allocate memory for lowpan6_ble driver", __func__);
        return NULL;
    }

    driver->base.post_attach = lowpan6_ble_post_attach;

    driver->chan = NULL;

    return driver;
}

esp_err_t lowpan6_ble_destroy(lowpan6_ble_driver_handle driver)
{
    return ESP_OK;
}

bool lowpan6_ble_connectable(struct ble_gap_disc_desc* disc)
{
    // Must advertise one of the following Protocol Data Units (PDUs) in order to be
    // "connectable". Other PDUs mean it's scannable or not connectable. The Directed
    // advertising type (DIR_IND) is maybe a bit suspect -- directed means it'll only accept
    // connection requests from a peer device. Theoretically we'd maybe want to confirm that we
    // are that peer device instead of just saying "yeah it's connectable".
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
        disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
    {
        return false;
    }

    // Must include an advertising field indicating support for the Internet Protocol Support
    // Service to be lowpan6_ble compatible.
    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to parse fields; rc=%d", __func__, rc);
        return false;
    }

    for (int i = 0; i < fields.num_uuids16; ++i)
    {
        if (ble_uuid_u16(&fields.uuids16[i].u) == LOWPAN6_BLE_SERVICE_UUID_IPSS)
        {
            return true;
        }
    }

    return false;
}

esp_err_t lowpan6_ble_connect(
    lowpan6_ble_driver_handle handle,
    ble_addr_t* addr,
    int32_t timeout_ms,
    lowpan6_ble_event_handler cb,
    void* userdata
)
{
    struct lowpan6_ble_driver* driver = (struct lowpan6_ble_driver*)handle;
    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cb)
    {
        driver->cb       = cb;
        driver->userdata = userdata;
    }
    else
    {
        driver->cb       = NULL;
        driver->userdata = NULL;
    }

    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to automatically infer address type; rc=%d", __func__, rc);
        return ESP_FAIL;
    }

    rc = ble_gap_connect(own_addr_type, addr, timeout_ms, NULL, on_gap_event, handle);
    if (rc != 0)
    {
        ESP_LOGE(
            TAG,
            "(%s) failed to connect to device; addr_type=%d addr=%s rc=%d",
            __func__,
            own_addr_type,
            debug_print_ble_addr(addr),
            rc
        );
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t lowpan6_ble_create_server(
    lowpan6_ble_driver_handle handle,
    lowpan6_ble_event_handler cb,
    void* userdata
)
{
    struct lowpan6_ble_driver* driver = (struct lowpan6_ble_driver*)handle;
    if (driver == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (cb)
    {
        driver->cb       = cb;
        driver->userdata = userdata;
    }
    else
    {
        driver->cb       = NULL;
        driver->userdata = NULL;
    }

    int rc =
        ble_l2cap_create_server(LOWPAN6_BLE_IPSP_PSM, LOWPAN6_BLE_IPSP_MTU, on_l2cap_event, driver);

    if (rc != 0)
    {
        ESP_LOGE(TAG, "(%s) failed to create L2CAP server; rc=%d", __func__, rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ble_addr_to_link_local(ble_addr_t* ble_addr, ip6_addr_t* ip_addr)
{
    if (ble_addr == NULL || ip_addr == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t eui64_addr[8];
    nimble_addr_to_eui64(ble_addr, eui64_addr);

    ipv6_create_link_local_from_eui64(eui64_addr, ip_addr);

    return ESP_OK;
}
