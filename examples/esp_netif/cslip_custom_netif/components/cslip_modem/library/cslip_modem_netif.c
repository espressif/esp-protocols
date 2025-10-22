/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/esp_netif_net_stack.h"
#include "lwip/dns.h"
#include "lwip/ip6_addr.h"
#include "lwip/netif.h"
#include "netif/slipif.h"
#include "lwip/sio.h"

static const char *TAG = "cslip-modem-netif";

esp_err_t cslip_modem_netif_stop(esp_netif_t *esp_netif)
{
    struct netif *netif = esp_netif_get_netif_impl(esp_netif);
    ESP_LOGI(TAG, "%s: Stopped CSLIP connection: lwip netif:%p", __func__, netif);
    netif_set_link_down(netif);
    return ESP_OK;
}

esp_err_t cslip_modem_netif_start(esp_netif_t *esp_netif, esp_ip6_addr_t *addr)
{
    struct netif *netif = esp_netif_get_netif_impl(esp_netif);
    ESP_LOGI(TAG, "%s: Starting CSLIP interface: lwip netif:%p", __func__, netif);
    netif_set_up(netif);
    netif_set_link_up(netif);
#if CONFIG_LWIP_IPV6
    int8_t addr_index = 0;
    netif_ip6_addr_set(netif, addr_index, (ip6_addr_t *)addr);
    netif_ip6_addr_set_state(netif, addr_index, IP6_ADDR_VALID);
#endif
    return ESP_OK;
}

void esp_netif_lwip_cslip_input(void *h, void *buffer, unsigned int len, void *eb)
{
    struct netif *netif = h;

    ESP_LOGD(TAG, "%s", __func__);
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_DEBUG);

    const int max_batch = 255;
    int sent = 0;
    while (sent < len) {
        int batch = (len - sent) > max_batch ? max_batch : (len - sent);
        slipif_received_bytes(netif, buffer + sent, batch);
        sent += batch;
    }

    for (int i = 0; i < len; i++) {
        slipif_process_rxqueue(netif);
    }
}

void cslip_modem_netif_raw_write(esp_netif_t *netif, void *buffer, size_t len)
{
    struct netif *lwip_netif = esp_netif_get_netif_impl(netif);
    ESP_LOGD(TAG, "%s", __func__);

    struct pbuf p = {
        .next = NULL,
        .payload = buffer,
        .tot_len = len,
        .len = len,
    };

#if CONFIG_LWIP_IPV6
    lwip_netif->output_ip6(lwip_netif, &p, NULL);
#else
    lwip_netif->output(lwip_netif, &p, NULL);
#endif
}

static esp_netif_t *get_netif_with_esp_index(int index)
{
    esp_netif_t *netif = NULL;
    int counter = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
#else
    while ((netif = esp_netif_next(netif)) != NULL) {
#endif
        if (counter == index) {
            return netif;
        }
        counter++;
    }
    return NULL;
}

static int get_esp_netif_index(esp_netif_t *esp_netif)
{
    esp_netif_t *netif = NULL;
    int counter = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
#else
    while ((netif = esp_netif_next(netif)) != NULL) {
#endif
        if (esp_netif == netif) {
            return counter;
        }
        counter++;
    }
    return -1;
}

static err_t esp_cslipif_init(struct netif *netif)
{
    if (netif == NULL) {
        return ERR_IF;
    }
    esp_netif_t *esp_netif = netif->state;
    int esp_index = get_esp_netif_index(esp_netif);
    if (esp_index < 0) {
        return ERR_IF;
    }

    netif->state = (void *)esp_index;

    return slipif_init(netif);
}

const struct esp_netif_netstack_config s_netif_config_cslip = {
    .lwip = {
        .init_fn = esp_cslipif_init,
        .input_fn = esp_netif_lwip_cslip_input,
    }
};

const esp_netif_netstack_config_t *netstack_default_cslip = &s_netif_config_cslip;

sio_fd_t sio_open(uint8_t devnum)
{
    ESP_LOGD(TAG, "Opening device: %d\r\n", devnum);

    esp_netif_t *esp_netif = get_netif_with_esp_index(devnum);
    if (!esp_netif) {
        ESP_LOGE(TAG, "didn't find esp-netif with index=%d\n", devnum);
        return NULL;
    }

    return esp_netif;
}

void sio_send(uint8_t c, sio_fd_t fd)
{
    esp_netif_t *esp_netif = fd;

    ESP_LOGD(TAG, "%s", __func__);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &c, 1, ESP_LOG_DEBUG);

    esp_err_t ret = esp_netif_transmit(esp_netif, &c, 1);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "%s: uart_write_bytes error %i", __func__, ret);
    }
}
