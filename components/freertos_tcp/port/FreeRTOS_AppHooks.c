/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_random.h"
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_DNS.h"

#ifndef CONFIG_AFPT_LOCAL_HOSTNAME
#define CONFIG_AFPT_LOCAL_HOSTNAME    "espressif"
#endif

#define AFPT_HOSTNAME_MAX_LEN    32

static char s_hostname[AFPT_HOSTNAME_MAX_LEN] = CONFIG_AFPT_LOCAL_HOSTNAME;


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

/* Returns global hostname for all network interfaces.
 * FreeRTOS+TCP DHCP queries this hook (not per-netif); keep in sync via
 * vApplicationSetHostname() / esp_netif_set_hostname(). */
const char *pcApplicationHostnameHook(void)
{
    return s_hostname;
}

void vApplicationSetHostname(const char *pcHostname)
{
    if (pcHostname == NULL) {
        return;
    }
    strncpy(s_hostname, pcHostname, sizeof(s_hostname) - 1);
    s_hostname[sizeof(s_hostname) - 1] = '\0';
}
