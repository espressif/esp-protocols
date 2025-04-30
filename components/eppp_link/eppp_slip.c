/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif_ppp.h"
#include "eppp_link.h"
#include "esp_netif_net_stack.h"
#include "lwip/esp_netif_net_stack.h"
#include "lwip/prot/ethernet.h"

#define TAG "slip"
#define SLIP_MAX_SIZE 1500

void esp_netif_lwip_slip_input(void *h, void *buffer, unsigned int len, void *eb)
{
    struct netif *netif = h;
    struct pbuf *p;

    ESP_LOGD(TAG, "%s", __func__);
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_DEBUG);
    p = pbuf_alloc(PBUF_RAW, len + SIZEOF_ETH_HDR, PBUF_RAM);
    pbuf_remove_header(p, SIZEOF_ETH_HDR);
    memcpy(p->payload, buffer, len);

    if (netif->input(p, netif) != ERR_OK) {
        pbuf_free(p);
    }
//    // Update slip netif with data
//    const int max_batch = 255;
//    int sent = 0;
//    while (sent < len) {
//        int batch = (len - sent) > max_batch ? max_batch : (len - sent);
//        slipif_received_bytes(netif, buffer + sent, batch);
//        sent += batch;
//    }
//
//    // Process incoming bytes
//    for (int i = 0; i < len; i++) {
//        slipif_process_rxqueue(netif);
//    }
}

static err_t
slipif_output(struct netif *netif, struct pbuf *p)
{
//    struct slipif_priv *priv;
//    struct pbuf *q;
//    u16_t i;
//    u8_t c;
    printf("output %d\n", p->len);
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    LWIP_ASSERT("netif->state != NULL", (netif->state != NULL));
    LWIP_ASSERT("p != NULL", (p != NULL));

//    LWIP_DEBUGF(SLIP_DEBUG, ("slipif_output: sending %"U16_F" bytes\n", p->tot_len));
//    priv = (struct slipif_priv *)netif->state;
//
//    /* Send pbuf out on the serial I/O device. */
//    /* Start with packet delimiter. */
//    sio_send(SLIP_END, priv->sd);
//
//    for (q = p; q != NULL; q = q->next) {
//        for (i = 0; i < q->len; i++) {
//            c = ((u8_t *)q->payload)[i];
//            switch (c) {
//                case SLIP_END:
//                    /* need to escape this byte (0xC0 -> 0xDB, 0xDC) */
//                    sio_send(SLIP_ESC, priv->sd);
//                    sio_send(SLIP_ESC_END, priv->sd);
//                    break;
//                case SLIP_ESC:
//                    /* need to escape this byte (0xDB -> 0xDB, 0xDD) */
//                    sio_send(SLIP_ESC, priv->sd);
//                    sio_send(SLIP_ESC_ESC, priv->sd);
//                    break;
//                default:
//                    /* normal byte - no need for escaping */
//                    sio_send(c, priv->sd);
//                    break;
//            }
//        }
//    }
//    /* End with packet delimiter. */
    esp_netif_transmit(netif->state, p->payload, p->len);
//    sio_send(SLIP_END, priv->sd);
    return ERR_OK;
}

static err_t
slipif_output_v4(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    LWIP_UNUSED_ARG(ipaddr);
    return slipif_output(netif, p);
}
static err_t
slipif_output_v6(struct netif *netif, struct pbuf *p, const ip6_addr_t *ipaddr)
{
    LWIP_UNUSED_ARG(ipaddr);
    return slipif_output(netif, p);
}

static err_t esp_slipif_init(struct netif *netif)
{
    if (netif == NULL) {
        return ERR_IF;
    }
    netif->name[0] = 's';
    netif->name[1] = 'l';
#if LWIP_IPV4
    netif->output = slipif_output_v4;
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
    netif->output_ip6 = slipif_output_v6;
#endif /* LWIP_IPV6 */
    netif->mtu = SLIP_MAX_SIZE;

    return ERR_OK;
//    esp_netif_t *esp_netif = netif->state;
//    int esp_index = get_esp_netif_index(esp_netif);
//    if (esp_index < 0) {
//        return ERR_IF;
//    }
//
//    // Store netif index in net interface for SIO open command to abstract the dev
//    netif->state = (void *) esp_index;
//
//    return slipif_init(netif);
}

const struct esp_netif_netstack_config s_netif_config_slip = {
    .lwip = {
        .init_fn = esp_slipif_init,
        .input_fn = esp_netif_lwip_slip_input,
    }
};

const esp_netif_netstack_config_t *netstack_default_slip = &s_netif_config_slip;
