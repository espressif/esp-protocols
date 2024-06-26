/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
//#include <lwip/esp_netif_net_stack.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"

#include "esp_log.h"
#include "interface.h"
#include "esp_netif.h"

//static const char *TAG = "freertos_tcp_netif";

NetworkInterface_t *pxESP32_Eth_FillInterfaceDescriptor(BaseType_t xEMACIndex, NetworkInterface_t *pxInterface);
esp_err_t xESP32_Eth_NetworkInterfaceInput(NetworkInterface_t *pxInterface, void *buffer, size_t len, void *eb);

static NetworkInterface_t *init(BaseType_t xEMACIndex, NetworkInterface_t *pxInterface)
{
    return pxESP32_Eth_FillInterfaceDescriptor(xEMACIndex, pxInterface);
}

static esp_err_t input(NetworkInterface_t *netif, void *buffer, size_t len, void *eb)
{
    return xESP32_Eth_NetworkInterfaceInput(netif, buffer, len, eb);
}

void esp_netif_netstack_buf_ref(void *pbuf)
{
}

void esp_netif_netstack_buf_free(void *pbuf)
{
}

static const struct esp_netif_netstack_config s_netif_config = {
    .init_fn = init,
    .input_fn = input
};

const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_wifi_sta = &s_netif_config;
const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_wifi_ap  = &s_netif_config;
const esp_netif_netstack_config_t *_g_esp_netif_netstack_default_eth = &s_netif_config;
