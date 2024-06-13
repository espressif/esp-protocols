/*
 * SPDX-FileCopyrightText: 2018-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_DNS.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"

static const char *TAG = "NetInterface";

static BaseType_t xESP32_Eth_NetworkInterfaceInitialise(NetworkInterface_t *pxInterface);

static BaseType_t xESP32_Eth_NetworkInterfaceOutput(NetworkInterface_t *pxInterface,
                                                    NetworkBufferDescriptor_t *const pxDescriptor,
                                                    BaseType_t xReleaseAfterSend);

static BaseType_t xESP32_Eth_GetPhyLinkStatus(NetworkInterface_t *pxInterface);

NetworkInterface_t *pxESP32_Eth_FillInterfaceDescriptor(BaseType_t xEMACIndex,
                                                        NetworkInterface_t *pxInterface);

/*-----------------------------------------------------------*/

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE != 0 )

/* Do not call the following function directly. It is there for downward compatibility.
 * The function FreeRTOS_IPInit() will call it to initialice the interface and end-point
 * objects.  See the description in FreeRTOS_Routing.h. */
NetworkInterface_t *pxFillInterfaceDescriptor(BaseType_t xEMACIndex,
                                              NetworkInterface_t *pxInterface)
{
    return pxESP32_Eth_FillInterfaceDescriptor(xEMACIndex, pxInterface);
}

#endif
/*-----------------------------------------------------------*/


NetworkInterface_t *pxESP32_Eth_FillInterfaceDescriptor(BaseType_t xEMACIndex,
                                                        NetworkInterface_t *pxInterface)
{
    static char pcName[ 8 ];

    /* This function pxESP32_Eth_FillInterfaceDescriptor() adds a network-interface.
     * Make sure that the object pointed to by 'pxInterface'
     * is declared static or global, and that it will remain to exist. */

    snprintf(pcName, sizeof(pcName), "eth%ld", xEMACIndex);

    memset(pxInterface, '\0', sizeof(*pxInterface));
    pxInterface->pcName = pcName;                    /* Just for logging, debugging. */
    pxInterface->pfInitialise = xESP32_Eth_NetworkInterfaceInitialise;
    pxInterface->pfOutput = xESP32_Eth_NetworkInterfaceOutput;
    pxInterface->pfGetPhyLinkStatus = xESP32_Eth_GetPhyLinkStatus;

    FreeRTOS_AddNetworkInterface(pxInterface);

    return pxInterface;
}
/*-----------------------------------------------------------*/

static BaseType_t xESP32_Eth_NetworkInterfaceInitialise(NetworkInterface_t *pxInterface)
{
    return pxInterface->bits.bInterfaceUp ? pdTRUE : pdFALSE;
}

static BaseType_t xESP32_Eth_GetPhyLinkStatus(NetworkInterface_t *pxInterface)
{
    BaseType_t xResult = pdFALSE;

    if (pxInterface->bits.bInterfaceUp) {
        xResult = pdTRUE;
    }

    return xResult;
}

static BaseType_t xESP32_Eth_NetworkInterfaceOutput(NetworkInterface_t *pxInterface,
                                                    NetworkBufferDescriptor_t *const pxDescriptor,
                                                    BaseType_t xReleaseAfterSend)
{
    if ((pxDescriptor == NULL) || (pxDescriptor->pucEthernetBuffer == NULL) || (pxDescriptor->xDataLength == 0)) {
        ESP_LOGE(TAG, "Invalid params");
        return pdFALSE;
    }

    esp_err_t ret;

    if (!(pxInterface->bits.bInterfaceUp)) {
        ESP_LOGD(TAG, "Interface down");
        ret = ESP_FAIL;
    } else {
        esp_netif_t *esp_netif = pxInterface->pvArgument;
        ret = esp_netif_transmit(esp_netif, pxDescriptor->pucEthernetBuffer, pxDescriptor->xDataLength);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to tx buffer %p, len %d, err %d", pxDescriptor->pucEthernetBuffer, pxDescriptor->xDataLength, ret);
        }
        ESP_LOG_BUFFER_HEXDUMP(TAG, pxDescriptor->pucEthernetBuffer, pxDescriptor->xDataLength, ESP_LOG_VERBOSE);
    }

#if ( ipconfigHAS_PRINTF != 0 )
    {
        /* Call a function that monitors resources: the amount of free network
         * buffers and the amount of free space on the heap.  See FreeRTOS_IP.c
         * for more detailed comments. */
        vPrintResourceStats();
    }
#endif /* ( ipconfigHAS_PRINTF != 0 ) */

    if (xReleaseAfterSend == pdTRUE) {
        vReleaseNetworkBufferAndDescriptor(pxDescriptor);
    }

    return ret == ESP_OK ? pdTRUE : pdFALSE;
}

esp_err_t xESP32_Eth_NetworkInterfaceInput(NetworkInterface_t *pxInterface, void *buffer, size_t len, void *eb)
{
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    IPStackEvent_t xRxEvent = { eNetworkRxEvent, NULL };
    const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS(250);

#if ( ipconfigHAS_PRINTF != 0 )
    {
        vPrintResourceStats();
    }
#endif /* ( ipconfigHAS_PRINTF != 0 ) */

    if (eConsiderFrameForProcessing(buffer) != eProcessBuffer) {
        ESP_LOGD(TAG, "Dropping packet");
        esp_netif_free_rx_buffer(pxInterface->pvArgument, eb);
        return ESP_OK;
    }

    pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(len, xDescriptorWaitTime);

    if (pxNetworkBuffer != NULL) {
        /* Set the packet size, in case a larger buffer was returned. */
        pxNetworkBuffer->xDataLength = len;
        pxNetworkBuffer->pxInterface = pxInterface;
        pxNetworkBuffer->pxEndPoint = FreeRTOS_MatchingEndpoint(pxInterface, buffer);

        /* Copy the packet data. */
        memcpy(pxNetworkBuffer->pucEthernetBuffer, buffer, len);
        xRxEvent.pvData = (void *) pxNetworkBuffer;

        if (xSendEventStructToIPTask(&xRxEvent, xDescriptorWaitTime) == pdFAIL) {
            ESP_LOGE(TAG, "Failed to enqueue packet to network stack %p, len %d", buffer, len);
            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
            return ESP_FAIL;
        }

        esp_netif_free_rx_buffer(pxInterface->pvArgument, eb);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to get buffer descriptor");
        return ESP_FAIL;
    }
}

void vNetworkNotifyIFUp(NetworkInterface_t *pxInterface)
{
    pxInterface->bits.bInterfaceUp = 1;
    IPStackEvent_t xRxEvent = { eNetworkDownEvent, NULL };
    xRxEvent.pvData = pxInterface;
    xSendEventStructToIPTask(&xRxEvent, 0);
}
