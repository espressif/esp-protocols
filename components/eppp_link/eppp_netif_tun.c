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
#include "esp_netif_net_stack.h"
#include "lwip/esp_netif_net_stack.h"
#include "lwip/prot/ethernet.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"
#include "esp_check.h"
#include "esp_idf_version.h"

#if defined(CONFIG_ESP_NETIF_RECEIVE_REPORT_ERRORS)
typedef esp_err_t esp_netif_recv_ret_t;
#define ESP_NETIF_OPTIONAL_RETURN_CODE(expr) expr
#else
typedef void esp_netif_recv_ret_t;
#define ESP_NETIF_OPTIONAL_RETURN_CODE(expr)
#endif // CONFIG_ESP_NETIF_RECEIVE_REPORT_ERRORS

static const char *TAG = "eppp_tun_netif";

static esp_netif_recv_ret_t tun_input(void *h, void *buffer, unsigned int len, void *eb)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    struct netif *netif = h;
    struct pbuf *p = NULL;

    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer, len, ESP_LOG_VERBOSE);
    // need to alloc extra space for the ETH header to support possible packet forwarding
    ESP_GOTO_ON_FALSE(p = pbuf_alloc(PBUF_RAW, len + SIZEOF_ETH_HDR, PBUF_RAM), ESP_ERR_NO_MEM, err, TAG, "pbuf_alloc failed");
    ESP_GOTO_ON_FALSE(pbuf_remove_header(p, SIZEOF_ETH_HDR) == 0, ESP_FAIL, err, TAG, "pbuf_remove_header failed");
    memcpy(p->payload, buffer, len);
    ESP_GOTO_ON_FALSE(netif->input(p, netif) == ERR_OK, ESP_FAIL, err, TAG, "failed to input packet to lwip");
    return ESP_NETIF_OPTIONAL_RETURN_CODE(ESP_OK);
err:
    if (p) {
        pbuf_free(p);
    }
    return ESP_NETIF_OPTIONAL_RETURN_CODE(ret);
}

static err_t tun_output(struct netif *netif, struct pbuf *p)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    LWIP_ASSERT("netif->state != NULL", (netif->state != NULL));
    LWIP_ASSERT("p != NULL", (p != NULL));
    esp_err_t ret = esp_netif_transmit(netif->state, p->payload, p->len);
    switch (ret) {
    case ESP_OK:
        return ERR_OK;
    case ESP_ERR_NO_MEM:
        return ERR_MEM;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    case ESP_ERR_ESP_NETIF_TX_FAILED:
        return ERR_BUF;
#endif
    case ESP_ERR_INVALID_ARG:
        return ERR_ARG;
    default:
        return ERR_IF;
    }
}

static err_t tun_output_v4(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
    LWIP_UNUSED_ARG(ipaddr);
    return tun_output(netif, p);
}
#if LWIP_IPV6
static err_t tun_output_v6(struct netif *netif, struct pbuf *p, const ip6_addr_t *ipaddr)
{
    LWIP_UNUSED_ARG(ipaddr);
    return tun_output(netif, p);
}
#endif
static err_t tun_init(struct netif *netif)
{
    if (netif == NULL) {
        return ERR_IF;
    }
    netif->name[0] = 't';
    netif->name[1] = 'u';
#if LWIP_IPV4
    netif->output = tun_output_v4;
#endif
#if LWIP_IPV6
    netif->output_ip6 = tun_output_v6;
#endif
    netif->mtu = 1500;
    return ERR_OK;
}

static const struct esp_netif_netstack_config s_config_tun = {
    .lwip = {
        .init_fn = tun_init,
        .input_fn = tun_input,
    }
};

const esp_netif_netstack_config_t *g_eppp_netif_config_tun = &s_config_tun;

static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGD(TAG, "%" PRIu32 " bytes from %s icmp_seq=%" PRIu16 " ttl=%" PRIu16 " time=%" PRIu32 " ms\n",
             recv_len, ipaddr_ntoa((ip_addr_t *)&target_addr), seqno, ttl, elapsed_time);
    if (esp_ping_stop(hdl) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop ping session");
        // continue to delete the session
    }
    esp_ping_delete_session(hdl);
    ESP_LOGI(TAG, "PING success -> stop and post IP");
    esp_netif_t *netif = (esp_netif_t *)args;
    esp_netif_ip_info_t ip = {0};
    esp_netif_get_ip_info(netif, &ip);
    esp_netif_set_ip_info(netif, &ip);
}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGD(TAG, "From %s icmp_seq=%" PRIu16 "timeout", ipaddr_ntoa((ip_addr_t *)&target_addr), seqno);
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ip_addr_t target_addr;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    uint32_t loss;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));

    if (transmitted > 0) {
        loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
    } else {
        loss = 0;
    }
    if (IP_IS_V4(&target_addr)) {
        ESP_LOGD(TAG, "\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
    }
#if LWIP_IPV6
    else {
        ESP_LOGD(TAG, "\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
    }
#endif
    ESP_LOGI(TAG, "%" PRIu32 " packets transmitted, %" PRIu32 " received, %" PRIu32 "%% packet loss, time %" PRIu32 "ms\n",
             transmitted, received, loss, total_time_ms);
    esp_ping_delete_session(hdl);
}

esp_err_t eppp_check_connection(esp_netif_t *netif)
{
    esp_err_t ret = ESP_OK;
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
#if CONFIG_LOG_MAXIMUM_LEVEL > 3
    config.task_stack_size += 1024;  // Some additional stack needed for verbose logs
#endif
    config.count = 100;
    ESP_LOGI(TAG, "Checking connection on EPPP interface #%" PRIu32, config.interface);
    ip_addr_t target_addr = {0};
    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(netif, &ip);
#if LWIP_IPV6
    target_addr.u_addr.ip4.addr =  ip.gw.addr;
#else
    target_addr.addr =  ip.gw.addr;
#endif
    config.target_addr = target_addr;
    esp_ping_callbacks_t cbs = {
        .cb_args = netif,
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end
    };
    esp_ping_handle_t ping;
    ESP_GOTO_ON_ERROR(esp_ping_new_session(&config, &cbs, &ping), err, TAG, "Failed to create ping session");
    ESP_GOTO_ON_ERROR(esp_ping_start(ping), err, TAG, "Failed to start ping session");
    ESP_LOGI(TAG, "Ping started");
    return ret;
err:
    ESP_LOGE(TAG, "Failed to start connection check");
    return ret;
}
