/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <inttypes.h>
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "interface.h"
#include "NetworkInterface.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_IP_Utils.h"
#include "FreeRTOS_DHCP.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_ENDPOINTS_PER_NETIF     1  // single interface, single endpoint

static const char *TAG = "esp_netif_AFpT";

static bool s_netif_initialized = false;
static bool s_FreeRTOS_IP_started = false;
static SemaphoreHandle_t s_tcpip_exec_mutex;

struct esp_netif_stack {
    esp_netif_netstack_config_t config;
    char if_name[8];
    NetworkInterface_t aft_netif;
    NetworkEndPoint_t endpoints[MAX_ENDPOINTS_PER_NETIF];
};

struct esp_netif_obj {
    uint8_t mac[6];
    // io driver related
    void *driver_handle;
    esp_err_t (*driver_transmit)(void *h, void *buffer, size_t len);
    void (*driver_free_rx_buffer)(void *h, void *buffer);

    // stack related
    struct esp_netif_stack *net_stack;
    bool phy_link_up;

    // misc flags, types, keys, priority
    esp_netif_flags_t flags;
    char *hostname;
    char *if_key;
    char *if_desc;
    int route_prio;
    uint32_t got_ip_event;
    uint32_t lost_ip_event;
    bool had_ip;
    esp_netif_dhcp_status_t dhcpc_status;
    esp_netif_ip_info_t last_ip_info;
};

static inline void ip4_to_afpt_ip(const esp_ip4_addr_t *ip, uint8_t afpt_ip[4])
{
    afpt_ip[0] = esp_ip4_addr1(ip);
    afpt_ip[1] = esp_ip4_addr2(ip);
    afpt_ip[2] = esp_ip4_addr3(ip);
    afpt_ip[3] = esp_ip4_addr4(ip);
}

static void esp_netif_unlink_netstack(struct esp_netif_stack *net_stack);

struct esp_netif_stack *esp_netif_new_netstack_if(esp_netif_t *esp_netif, const esp_netif_inherent_config_t *base_cfg, const esp_netif_netstack_config_t *cfg)
{
    static int netif_count = 0;
    struct esp_netif_stack *netif = calloc(1, sizeof(struct esp_netif_stack));
    if (!netif) {
        return NULL;
    }
    netif->config.init_fn = cfg->init_fn;
    BaseType_t xEMACIndex = netif_count++;
    if (!cfg->init_fn(xEMACIndex, &netif->aft_netif)) {
        ESP_LOGE(TAG, "Failed to initialize network interface");
        /* init_fn may have already registered the interface with FreeRTOS+TCP */
        esp_netif_unlink_netstack(netif);
        free(netif);
        return NULL;
    }

    /* Per-interface name (FillInterfaceDescriptor must not share one static). */
    snprintf(netif->if_name, sizeof(netif->if_name), "eth%ld", (long) xEMACIndex);
    netif->aft_netif.pcName = netif->if_name;

    /* Set before FreeRTOS_IPInit_Multi() so the IP task never sees a NULL handle. */
    netif->aft_netif.pvArgument = esp_netif;
    netif->config.input_fn = cfg->input_fn;

    uint8_t ip[4] = { 0 }, mask[4] = { 0 }, gw[4] = { 0 }, dns[4] = { 0 };
    if (base_cfg->ip_info) {
        ip4_to_afpt_ip(&base_cfg->ip_info->ip, ip);
        ip4_to_afpt_ip(&base_cfg->ip_info->netmask, mask);
        ip4_to_afpt_ip(&base_cfg->ip_info->gw, gw);
    }
    FreeRTOS_FillEndPoint(&(netif->aft_netif), &(netif->endpoints[0]), ip, mask, gw, dns, base_cfg->mac);
    if (base_cfg->flags & ESP_NETIF_DHCP_CLIENT) {
        netif->endpoints[0].bits.bWantDHCP = pdTRUE;
    }
    netif->aft_netif.bits.bInterfaceUp = 0;
    if (!s_FreeRTOS_IP_started) {
        if (FreeRTOS_IPInit_Multi() != pdPASS) {
            ESP_LOGE(TAG, "FreeRTOS_IPInit_Multi failed");
            esp_netif_unlink_netstack(netif);
            free(netif);
            return NULL;
        }
        if (s_tcpip_exec_mutex == NULL) {
            s_tcpip_exec_mutex = xSemaphoreCreateMutex();
        }
        s_FreeRTOS_IP_started = true;
    }

    return netif;
}


esp_err_t esp_netif_receive(esp_netif_t *esp_netif, void *buffer, size_t len, void *eb)
{
    struct esp_netif_stack *netif = esp_netif->net_stack;
    return netif->config.input_fn(&netif->aft_netif, buffer, len, eb);
}

void esp_netif_set_ip4_addr(esp_ip4_addr_t *addr, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    memset(addr, 0, sizeof(esp_ip4_addr_t));
    addr->addr = esp_netif_htonl(esp_netif_ip4_makeu32(a, b, c, d));
}

char *esp_ip4addr_ntoa(const esp_ip4_addr_t *addr, char *buf, int buflen)
{
    char tmp[16];

    if (addr == NULL || buf == NULL || buflen <= 0) {
        return NULL;
    }
    FreeRTOS_inet_ntoa(addr->addr, tmp);
    strncpy(buf, tmp, (size_t) buflen - 1);
    buf[buflen - 1] = '\0';
    return buf;
}

esp_netif_iodriver_handle esp_netif_get_io_driver(esp_netif_t *esp_netif)
{
    return esp_netif->driver_handle;
}

esp_netif_t *esp_netif_get_handle_from_netif_impl(void *dev)
{
    NetworkInterface_t *netif = dev;
    return (esp_netif_t *)netif->pvArgument;
}

esp_err_t esp_netif_init(void)
{
    ESP_LOGI(TAG, "esp_netif AFpT initialization");
    if (s_netif_initialized) {
        ESP_LOGE(TAG, "esp-netif has already been initialized");
        return ESP_ERR_INVALID_ARG;
    }
    s_netif_initialized = true;
    ESP_LOGD(TAG, "esp-netif has been successfully initialized");
    return ESP_OK;
}

esp_err_t esp_netif_deinit(void)
{
    ESP_LOGI(TAG, "esp_netif AFpT deinit");
    if (!s_netif_initialized) {
        ESP_LOGE(TAG, "esp-netif has not been initialized yet");
        return ESP_ERR_INVALID_SIZE;
    }
    s_netif_initialized = false;
    ESP_LOGD(TAG, "esp-netif has been successfully deinitialized");
    return ESP_OK;
}

static void esp_netif_unlink_netstack(struct esp_netif_stack *net_stack)
{
    if (net_stack == NULL) {
        return;
    }

    NetworkInterface_t *pxInterface = &net_stack->aft_netif;
    NetworkEndPoint_t *pxEndPoint = &net_stack->endpoints[0];

    /* Unlink endpoint from the global endpoint list. */
    if (pxNetworkEndPoints == pxEndPoint) {
        pxNetworkEndPoints = pxEndPoint->pxNext;
    } else {
        for (NetworkEndPoint_t *pxIter = pxNetworkEndPoints; pxIter != NULL; pxIter = pxIter->pxNext) {
            if (pxIter->pxNext == pxEndPoint) {
                pxIter->pxNext = pxEndPoint->pxNext;
                break;
            }
        }
    }
    pxEndPoint->pxNext = NULL;
    pxEndPoint->pxNetworkInterface = NULL;
    pxInterface->pxEndPoint = NULL;

    /* Unlink interface from the global interface list. */
    if (pxNetworkInterfaces == pxInterface) {
        pxNetworkInterfaces = pxInterface->pxNext;
    } else {
        for (NetworkInterface_t *pxIter = pxNetworkInterfaces; pxIter != NULL; pxIter = pxIter->pxNext) {
            if (pxIter->pxNext == pxInterface) {
                pxIter->pxNext = pxInterface->pxNext;
                break;
            }
        }
    }
    pxInterface->pxNext = NULL;
    pxInterface->pvArgument = NULL;
}

static esp_err_t esp_netif_init_configuration(esp_netif_t *esp_netif, const esp_netif_config_t *cfg)
{
    // Basic esp_netif and lwip is a mandatory configuration and cannot be updated after esp_netif_new()
    if (cfg == NULL || cfg->base == NULL || cfg->stack == NULL) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }

    // Configure general esp-netif properties
    memcpy(esp_netif->mac, cfg->base->mac, sizeof(esp_netif->mac));

    // Setup main config parameters
    esp_netif->flags = cfg->base->flags;

    if (cfg->base->if_key) {
        esp_netif->if_key = strdup(cfg->base->if_key);
    }
    if (cfg->base->if_desc) {
        esp_netif->if_desc = strdup(cfg->base->if_desc);
    }
    if (cfg->base->route_prio) {
        esp_netif->route_prio = cfg->base->route_prio;
    }
    esp_netif->got_ip_event = cfg->base->get_ip_event;
    esp_netif->lost_ip_event = cfg->base->lost_ip_event;
    esp_netif->dhcpc_status = (cfg->base->flags & ESP_NETIF_DHCP_CLIENT) ?
                              ESP_NETIF_DHCP_INIT : ESP_NETIF_DHCP_STOPPED;
    esp_netif->hostname = strdup(CONFIG_AFPT_LOCAL_HOSTNAME);
    if (esp_netif->hostname == NULL) {
        return ESP_ERR_NO_MEM;
    }
    vApplicationSetHostname(esp_netif->hostname);

    // Network stack configs
    esp_netif->net_stack = esp_netif_new_netstack_if(esp_netif, cfg->base, cfg->stack);
    if (esp_netif->net_stack == NULL) {
        return ESP_ERR_ESP_NETIF_INIT_FAILED;
    }

    // Install IO functions only if provided -- connects driver and netif
    // this configuration could be updated after esp_netif_new(), typically in post_attach callback
    if (cfg->driver) {
        const esp_netif_driver_ifconfig_t *esp_netif_driver_config = cfg->driver;
        if (esp_netif_driver_config->handle) {
            esp_netif->driver_handle = esp_netif_driver_config->handle;
        }
        if (esp_netif_driver_config->transmit) {
            esp_netif->driver_transmit = esp_netif_driver_config->transmit;
        }
        if (esp_netif_driver_config->driver_free_rx_buffer) {
            esp_netif->driver_free_rx_buffer = esp_netif_driver_config->driver_free_rx_buffer;
        }
    }
    return ESP_OK;
}

esp_netif_t *esp_netif_new(const esp_netif_config_t *esp_netif_config)
{
    // mandatory configuration must be provided when creating esp_netif object
    if (esp_netif_config == NULL) {
        return NULL;
    }

    // Create parent esp-netif object
    esp_netif_t *esp_netif = calloc(1, sizeof(struct esp_netif_obj));
    if (!esp_netif) {
        return NULL;
    }

    // Configure the created object with provided configuration
    esp_err_t ret =  esp_netif_init_configuration(esp_netif, esp_netif_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initial configuration of esp_netif failed with %d", ret);
        esp_netif_destroy(esp_netif);
        return NULL;
    }

    return esp_netif;
}

void esp_netif_destroy(esp_netif_t *esp_netif)
{
    if (esp_netif) {
        if (esp_netif->net_stack) {
            FreeRTOS_NetworkDown(&esp_netif->net_stack->aft_netif);
            esp_netif_unlink_netstack(esp_netif->net_stack);
            free(esp_netif->net_stack);
            esp_netif->net_stack = NULL;
        }
        free(esp_netif->if_key);
        free(esp_netif->if_desc);
        free(esp_netif->hostname);
        free(esp_netif);
    }
}

esp_err_t esp_netif_attach(esp_netif_t *esp_netif, esp_netif_iodriver_handle driver_handle)
{
    esp_netif_driver_base_t *base_driver = driver_handle;

    esp_netif->driver_handle = driver_handle;
    if (base_driver->post_attach) {
        esp_err_t ret = base_driver->post_attach(esp_netif, driver_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Post-attach callback of driver(%p) failed with %d", driver_handle, ret);
            return ESP_ERR_ESP_NETIF_DRIVER_ATTACH_FAILED;
        }
    }
    return ESP_OK;
}

esp_err_t esp_netif_set_driver_config(esp_netif_t *esp_netif,
                                      const esp_netif_driver_ifconfig_t *driver_config)
{
    if (esp_netif == NULL || driver_config == NULL) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    esp_netif->driver_handle = driver_config->handle;
    esp_netif->driver_transmit = driver_config->transmit;
    esp_netif->driver_free_rx_buffer = driver_config->driver_free_rx_buffer;
    return ESP_OK;
}

esp_err_t esp_netif_set_mac(esp_netif_t *esp_netif, uint8_t ucMACAddress[])
{
    ESP_LOGI(TAG, "esp_netif_set_mac()");
    if (esp_netif == NULL || ucMACAddress == NULL) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    memcpy(esp_netif->mac, ucMACAddress, sizeof(esp_netif->mac));
    if (esp_netif->net_stack != NULL) {
        memcpy(esp_netif->net_stack->endpoints[0].xMACAddress.ucBytes, ucMACAddress,
               sizeof(esp_netif->net_stack->endpoints[0].xMACAddress.ucBytes));
    }
    return ESP_OK;
}

esp_err_t esp_netif_start(esp_netif_t *esp_netif)
{
    ESP_LOGI(TAG, "Netif started");
    if (esp_netif == NULL || esp_netif->net_stack == NULL) {
        return ESP_ERR_ESP_NETIF_IF_NOT_READY;
    }
    memcpy(esp_netif->net_stack->endpoints[0].xMACAddress.ucBytes, esp_netif->mac,
           sizeof(esp_netif->net_stack->endpoints[0].xMACAddress.ucBytes));
    return ESP_OK;
}


esp_err_t esp_netif_stop(esp_netif_t *esp_netif)
{
    ESP_LOGI(TAG, "Netif stopped");
    if (esp_netif == NULL || esp_netif->net_stack == NULL) {
        return ESP_ERR_ESP_NETIF_IF_NOT_READY;
    }

    NetworkEndPoint_t *ep = &esp_netif->net_stack->endpoints[0];
    NetworkInterface_t *pxInterface = &esp_netif->net_stack->aft_netif;

    /* Mirror lwIP stop: halt DHCP client and return it to INIT so the next
     * connected/start path can run esp_netif_dhcpc_start() again. */
    if ((esp_netif->flags & ESP_NETIF_DHCP_CLIENT) &&
            (esp_netif->dhcpc_status == ESP_NETIF_DHCP_STARTED)) {
        vDHCPStop(ep);
        esp_netif->dhcpc_status = ESP_NETIF_DHCP_INIT;
    }

    esp_netif->phy_link_up = false;
    pxInterface->bits.bInterfaceUp = pdFALSE_UNSIGNED;
    FreeRTOS_NetworkDown(pxInterface);
    return ESP_OK;
}

//
// IO translate functions
//
void esp_netif_free_rx_buffer(void *h, void *buffer)
{
    esp_netif_t *esp_netif = h;
    esp_netif->driver_free_rx_buffer(esp_netif->driver_handle, buffer);
}

esp_err_t esp_netif_transmit(esp_netif_t *esp_netif, void *data, size_t len)
{
    ESP_LOGD(TAG, "Transmitting data: ptr:%p, size:%lu", data, (long unsigned int) len);
    return (esp_netif->driver_transmit)(esp_netif->driver_handle, data, len);
}

esp_err_t esp_netif_dhcpc_stop(esp_netif_t *esp_netif)
{
    if (esp_netif == NULL || esp_netif->net_stack == NULL) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (esp_netif->dhcpc_status == ESP_NETIF_DHCP_STOPPED) {
        return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED;
    }

    NetworkEndPoint_t *ep = &esp_netif->net_stack->endpoints[0];
    ep->bits.bWantDHCP = pdFALSE;
    vDHCPStop(ep);
    esp_netif->dhcpc_status = ESP_NETIF_DHCP_STOPPED;
    return ESP_OK;
}

esp_err_t esp_netif_dhcpc_start(esp_netif_t *esp_netif)
{
    if (esp_netif == NULL || esp_netif->net_stack == NULL) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (!(esp_netif->flags & ESP_NETIF_DHCP_CLIENT)) {
        return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
    }
    if (esp_netif->dhcpc_status == ESP_NETIF_DHCP_STARTED) {
        return ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED;
    }

    NetworkEndPoint_t *ep = &esp_netif->net_stack->endpoints[0];
    ep->bits.bWantDHCP = pdTRUE;

    /* Kick the FreeRTOS+TCP DHCP state machine via the IP task. Required when
     * start() runs after esp_netif_up()'s FreeRTOS_NetworkDown has already
     * completed without DHCP (e.g. after an explicit stop). */
    if (esp_netif->phy_link_up) {
        ep->xDHCPData.eDHCPState = eInitialWait;
        if (xSendDHCPEvent(ep) != pdPASS) {
            ESP_LOGE(TAG, "Failed to queue DHCP start event");
            ep->bits.bWantDHCP = pdFALSE;
            return ESP_FAIL;
        }
    }

    esp_netif->dhcpc_status = ESP_NETIF_DHCP_STARTED;
    return ESP_OK;
}

esp_err_t esp_netif_dhcps_get_status(esp_netif_t *esp_netif, esp_netif_dhcp_status_t *status)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_dhcpc_get_status(esp_netif_t *esp_netif, esp_netif_dhcp_status_t *status)
{
    if (esp_netif == NULL || status == NULL || (esp_netif->flags & ESP_NETIF_DHCP_SERVER)) {
        return ESP_ERR_INVALID_ARG;
    }
    *status = esp_netif->dhcpc_status;
    return ESP_OK;
}

esp_err_t esp_netif_dhcps_start(esp_netif_t *esp_netif)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_dhcps_stop(esp_netif_t *esp_netif)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_set_hostname(esp_netif_t *esp_netif, const char *hostname)
{
    if (esp_netif == NULL || hostname == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    free(esp_netif->hostname);
    esp_netif->hostname = strdup(hostname);
    if (esp_netif->hostname == NULL) {
        return ESP_ERR_NO_MEM;
    }
    /* FreeRTOS+TCP DHCP uses a global hostname hook; keep it in sync. */
    vApplicationSetHostname(esp_netif->hostname);
    return ESP_OK;
}

esp_err_t esp_netif_get_hostname(esp_netif_t *esp_netif, const char **hostname)
{
    *hostname = esp_netif->hostname;
    return ESP_OK;
}

esp_err_t esp_netif_up(esp_netif_t *esp_netif)
{
    if (esp_netif == NULL || esp_netif->net_stack == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Netif going up");
    NetworkInterface_t *pxInterface = &esp_netif->net_stack->aft_netif;
    IPStackEvent_t xNetworkDownEvent = {
        .eEventType = eNetworkDownEvent,
        .pvData = pxInterface,
    };

    /* FreeRTOS+TCP brings the interface up by processing eNetworkDownEvent
     * (init + DHCP). FreeRTOS_NetworkDown() posts that with a zero timeout and
     * only marks a deferred pending flag on failure — under a full event queue
     * bring-up can lag indefinitely from the caller's point of view. */
    esp_netif->phy_link_up = true;
    pxInterface->bits.bInterfaceUp = ipFALSE_BOOL;

    if (xSendEventStructToIPTask(&xNetworkDownEvent, pdMS_TO_TICKS(1000)) == pdPASS) {
        pxInterface->bits.bCallDownEvent = ipFALSE_BOOL;
        return ESP_OK;
    }

    /* xSendEventStructToIPTask forces a zero timeout when already on the IP
     * task; fall back to FreeRTOS_NetworkDown so bCallDownEvent /
     * xNetworkDownEventPending are set for prvIPTask_CheckPendingEvents(). */
    if (xIsCallingFromIPTask() != pdFALSE) {
        FreeRTOS_NetworkDown(pxInterface);
        return ESP_OK;
    }

    esp_netif->phy_link_up = false;
    ESP_LOGE(TAG, "Failed to queue network bring-up event");
    return ESP_FAIL;
}

esp_err_t esp_netif_down(esp_netif_t *esp_netif)
{
    ESP_LOGI(TAG, "Netif going down");
    esp_netif->phy_link_up = false;
    FreeRTOS_NetworkDown(&esp_netif->net_stack->aft_netif);
    return ESP_OK;
}

bool esp_netif_is_netif_up(esp_netif_t *esp_netif)
{
    return esp_netif->phy_link_up;
}

esp_err_t esp_netif_get_old_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info)
{
    ESP_LOGD(TAG, "%s esp_netif:%p", __func__, esp_netif);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info)
{
    ESP_LOGD(TAG, "%s esp_netif:%p", __func__, esp_netif);
    uint32_t ulIPAddress = 0, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
    struct esp_netif_stack *netif = esp_netif->net_stack;
    FreeRTOS_GetEndPointConfiguration(&ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress, netif->aft_netif.pxEndPoint);
    ip_info->ip.addr = ulIPAddress;
    ip_info->netmask.addr = ulNetMask;
    ip_info->gw.addr = ulGatewayAddress;
    return ESP_OK;
}

bool esp_netif_is_valid_static_ip(esp_netif_ip_info_t *ip_info)
{
    return true;
}

esp_err_t esp_netif_set_old_ip_info(esp_netif_t *esp_netif, const esp_netif_ip_info_t *ip_info)
{
    ESP_LOGD(TAG, "%s esp_netif:%p", __func__, esp_netif);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_set_dns_info(esp_netif_t *esp_netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_get_dns_info(esp_netif_t *esp_netif, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_create_ip6_linklocal(esp_netif_t *esp_netif)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_get_ip6_linklocal(esp_netif_t *esp_netif, esp_ip6_addr_t *if_ip6)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_get_ip6_global(esp_netif_t *esp_netif, esp_ip6_addr_t *if_ip6)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_netif_flags_t esp_netif_get_flags(esp_netif_t *esp_netif)
{
    return esp_netif->flags;
}

const char *esp_netif_get_ifkey(esp_netif_t *esp_netif)
{
    return esp_netif->if_key;
}

const char *esp_netif_get_desc(esp_netif_t *esp_netif)
{
    return esp_netif->if_desc;
}

int32_t esp_netif_get_event_id(esp_netif_t *esp_netif, esp_netif_ip_event_type_t event_type)
{
    switch (event_type) {
    case ESP_NETIF_IP_EVENT_GOT_IP:
        return esp_netif->got_ip_event;
    case ESP_NETIF_IP_EVENT_LOST_IP:
        return esp_netif->lost_ip_event;
    default:
        return -1;
    }
}

esp_err_t esp_netif_dhcps_option(esp_netif_t *esp_netif, esp_netif_dhcp_option_mode_t opt_op, esp_netif_dhcp_option_id_t opt_id, void *opt_val,
                                 uint32_t opt_len)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_dhcpc_option(esp_netif_t *esp_netif, esp_netif_dhcp_option_mode_t opt_op, esp_netif_dhcp_option_id_t opt_id, void *opt_val,
                                 uint32_t opt_len)
{
    return ESP_ERR_NOT_SUPPORTED;
}

int esp_netif_get_netif_impl_index(esp_netif_t *esp_netif)
{
    return 0;
}

esp_err_t esp_netif_join_ip6_multicast_group(esp_netif_t *esp_netif, const esp_ip6_addr_t *addr)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_leave_ip6_multicast_group(esp_netif_t *esp_netif, const esp_ip6_addr_t *addr)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_add_ip6_address(esp_netif_t *esp_netif, const esp_ip6_addr_t addr, bool preferred)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_remove_ip6_address(esp_netif_t *esp_netif, const esp_ip6_addr_t *addr)
{
    return ESP_ERR_NOT_SUPPORTED;
}

int esp_netif_get_all_ip6(esp_netif_t *esp_netif, esp_ip6_addr_t if_ip6[])
{
    return 0;
}

#if ESP_IDF_VERSION_MAJOR >= 6
esp_ip6_addr_type_t esp_netif_ip6_get_addr_type(const esp_ip6_addr_t* ip6_addr)
#else
esp_ip6_addr_type_t esp_netif_ip6_get_addr_type(esp_ip6_addr_t *ip6_addr)
#endif
{
    return ESP_IP6_ADDR_IS_UNKNOWN;
}

/* Marshalled via iptraceNETWORK_EVENT_RECEIVED on the FreeRTOS+TCP IP task. */
#define ESP_NETIF_TCPIP_EXEC_EVENT    15  /* eSocketSetDeleteEvent + 1 */

typedef struct {
    esp_netif_callback_fn fn;
    void *ctx;
    SemaphoreHandle_t done;
    esp_err_t result;
} esp_netif_tcpip_exec_msg_t;

static esp_netif_tcpip_exec_msg_t *s_tcpip_exec_msg;

void vEspNetifProcessTcpipExecEvent(int eEvent)
{
    if (eEvent != ESP_NETIF_TCPIP_EXEC_EVENT || s_tcpip_exec_msg == NULL || s_tcpip_exec_msg->fn == NULL) {
        return;
    }
    s_tcpip_exec_msg->result = s_tcpip_exec_msg->fn(s_tcpip_exec_msg->ctx);
    (void) xSemaphoreGive(s_tcpip_exec_msg->done);
}

esp_err_t esp_netif_tcpip_exec(esp_netif_callback_fn fn, void *ctx)
{
    if (fn == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Before IP task exists, or when already on it, run inline. */
    if (!s_FreeRTOS_IP_started || s_tcpip_exec_mutex == NULL || xIsCallingFromIPTask() != pdFALSE) {
        return fn(ctx);
    }

    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (done == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_netif_tcpip_exec_msg_t msg = {
        .fn = fn,
        .ctx = ctx,
        .done = done,
        .result = ESP_FAIL,
    };

    if (xSemaphoreTake(s_tcpip_exec_mutex, portMAX_DELAY) != pdTRUE) {
        vSemaphoreDelete(done);
        return ESP_FAIL;
    }

    s_tcpip_exec_msg = &msg;
    IPStackEvent_t event = {
        .eEventType = (eIPEvent_t) ESP_NETIF_TCPIP_EXEC_EVENT,
        .pvData = NULL,
    };

    if (xSendEventStructToIPTask(&event, portMAX_DELAY) != pdPASS) {
        s_tcpip_exec_msg = NULL;
        xSemaphoreGive(s_tcpip_exec_mutex);
        vSemaphoreDelete(done);
        return ESP_FAIL;
    }

    (void) xSemaphoreTake(done, portMAX_DELAY);
    esp_err_t result = msg.result;
    s_tcpip_exec_msg = NULL;
    xSemaphoreGive(s_tcpip_exec_mutex);
    vSemaphoreDelete(done);
    return result;
}

esp_netif_t *esp_netif_find_if(esp_netif_find_predicate_t fn, void *ctx)
{
    for (NetworkInterface_t *netif = FreeRTOS_FirstNetworkInterface(); netif != NULL; netif = FreeRTOS_NextNetworkInterface(netif)) {
        esp_netif_t *esp_netif = netif->pvArgument;
        if (fn(esp_netif, ctx)) {
            return esp_netif;
        }
    }
    return NULL;
}

esp_err_t esp_netif_set_link_speed(esp_netif_t *esp_netif, uint32_t speed)
{
    return ESP_OK;
}

BaseType_t xEspNetif_IsPhyUp(NetworkInterface_t *pxInterface)
{
    if (pxInterface == NULL || pxInterface->pvArgument == NULL) {
        return pdFALSE;
    }
    esp_netif_t *esp_netif = pxInterface->pvArgument;
    return esp_netif->phy_link_up ? pdTRUE : pdFALSE;
}

/* Called by FreeRTOS+TCP when the network connects or disconnects. */
void vApplicationIPNetworkEventHook_Multi(eIPCallbackEvent_t eNetworkEvent,
                                          struct xNetworkEndPoint *pxEndPoint)
{
    if (eNetworkEvent == eNetworkUp) {
        uint32_t ulIPAddress = 0, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
        FreeRTOS_GetEndPointConfiguration(&ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress, pxEndPoint);

        esp_netif_t *esp_netif = NULL;
        if (pxEndPoint != NULL && pxEndPoint->pxNetworkInterface != NULL) {
            esp_netif = pxEndPoint->pxNetworkInterface->pvArgument;
        }
        if (esp_netif == NULL) {
            ESP_LOGE(TAG, "network up: missing esp_netif");
            return;
        }

        ESP_LOGI(TAG, "Network up, ip=0x%08" PRIx32, ulIPAddress);
        esp_netif_ip_info_t ip_info = {
            .ip.addr = ulIPAddress,
            .gw.addr = ulGatewayAddress,
            .netmask.addr = ulNetMask,
        };
        ip_event_got_ip_t evt = {
            .esp_netif = esp_netif,
            .ip_changed = (memcmp(&ip_info, &esp_netif->last_ip_info, sizeof(ip_info)) != 0),
            .ip_info = ip_info,
        };
        esp_netif->last_ip_info = ip_info;
        esp_netif->had_ip = true;
        esp_err_t ret = esp_event_post(IP_EVENT, esp_netif->got_ip_event, &evt, sizeof(evt), 0);
        if (ESP_OK != ret) {
            ESP_LOGE(TAG, "dhcpc cb: failed to post got ip event (%x)", ret);
        }
    } else {
        esp_netif_t *esp_netif = NULL;
        if (pxEndPoint != NULL && pxEndPoint->pxNetworkInterface != NULL) {
            esp_netif = pxEndPoint->pxNetworkInterface->pvArgument;
        }
        ESP_LOGI(TAG, "Network down");
        /* FreeRTOS_NetworkDown is also used for bring-up; only post lost-IP
         * if an address was previously held. */
        if (esp_netif != NULL && esp_netif->had_ip && esp_netif->lost_ip_event) {
            esp_netif->had_ip = false;
            memset(&esp_netif->last_ip_info, 0, sizeof(esp_netif->last_ip_info));
            ip_event_got_ip_t evt = {
                .esp_netif = esp_netif,
            };
            esp_err_t ret = esp_event_post(IP_EVENT, esp_netif->lost_ip_event, &evt, sizeof(evt), 0);
            if (ESP_OK != ret) {
                ESP_LOGE(TAG, "network down: failed to post lost ip event (%x)", ret);
            }
        }
    }
}

BaseType_t xApplicationDNSQueryHook_Multi(struct xNetworkEndPoint *pxEndPoint, const char *pcName)
{
    BaseType_t xReturn;

    esp_netif_t *esp_netif = pxEndPoint->pxNetworkInterface->pvArgument;
    /* Determine if a name lookup is for this node.  Two names are given
     * to this node: that returned by pcApplicationHostnameHook() and that set
     * by mainDEVICE_NICK_NAME. */
    if (strcasecmp(pcName, pcApplicationHostnameHook()) == 0) {
        xReturn = pdPASS;
    } else if ((esp_netif->hostname != NULL) && (strcasecmp(pcName, esp_netif->hostname) == 0)) {
        xReturn = pdPASS;
    } else {
        xReturn = pdFAIL;
    }
    return xReturn;
}
