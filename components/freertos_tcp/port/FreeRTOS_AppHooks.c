/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_random.h"
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_DNS.h"
#include "NetworkBufferManagement.h"

#define mainHOST_NAME                                 "espressif"


UBaseType_t uxRand(void)
{
    return esp_random();
}

extern uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
                                                   uint16_t usSourcePort,
                                                   uint32_t ulDestinationAddress,
                                                   uint16_t usDestinationPort)
{
    (void) ulSourceAddress;
    (void) usSourcePort;
    (void) ulDestinationAddress;
    (void) usDestinationPort;

    return uxRand();
}

BaseType_t xApplicationGetRandomNumber(uint32_t *pulNumber)
{
    *(pulNumber) = uxRand();
    return pdTRUE;
}

#if ( ipconfigPROCESS_CUSTOM_ETHERNET_FRAMES != 0 )
eFrameProcessingResult_t eApplicationProcessCustomFrameHook(NetworkBufferDescriptor_t *const pxNetworkBuffer)
{
    (void)(pxNetworkBuffer);
    return eProcessBuffer;
}
#endif

void vApplicationPingReplyHook(ePingReplyStatus_t eStatus,
                               uint16_t usIdentifier)
{
}

eDHCPCallbackAnswer_t xApplicationDHCPHook_Multi(eDHCPCallbackPhase_t eDHCPPhase,
                                                 struct xNetworkEndPoint *pxEndPoint,
                                                 IP_Address_t *pxIPAddress)
{
    (void) eDHCPPhase;
    (void) pxIPAddress;

    return eDHCPContinue;
}

/* MVP: Returns global hostname for all network interfaces.
 * Future enhancement: Make this netif-specific once FreeRTOS-Plus-TCP supports it. */
const char *pcApplicationHostnameHook(void)
{
    return mainHOST_NAME;
}

/*-----------------------------------------------------------*/
/* Network buffer management
 * MVP: Uses static buffer allocation for predictable memory usage and deterministic behavior.
 * Future enhancement: Add Kconfig option to support dynamic allocation for memory-constrained systems. */
#define NETWORK_BUFFER_SIZE    1536
static uint8_t ucBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ][ NETWORK_BUFFER_SIZE ];
void vNetworkInterfaceAllocateRAMToBuffers(NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ])
{
    BaseType_t x;

    for (x = 0; x < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; x++) {
        /* pucEthernetBuffer is set to point ipBUFFER_PADDING bytes in from the
         * beginning of the allocated buffer. */
        pxNetworkBuffers[ x ].pucEthernetBuffer = &(ucBuffers[ x ][ ipBUFFER_PADDING ]);

        /* The following line is also required, but will not be required in
         * future versions. */
        *((uint32_t *) &ucBuffers[ x ][ 0 ]) = (uint32_t) & (pxNetworkBuffers[ x ]);
    }
}
