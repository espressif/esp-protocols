/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_console.h"
#include "esp_event.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip6.h"
#include "lwip/opt.h"
#include "ethernet_init.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif


typedef struct netif_op_ netif_op;

typedef struct netif_op_ {
    char *name;
    esp_err_t (*operation)(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
    int arg_cnt;
    int start_index;
    char *help;
    int netif_flag;
} netif_op;

esp_err_t ifcfg_help_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_print_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_lwip_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_basic_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_ip_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_napt_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_addr_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_netif_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);
esp_err_t ifcfg_eth_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif);

static const char *TAG = "console_ifconfig";

netif_op cmd_list[] = {
    {.name = "help",      .operation = ifcfg_help_op,   .arg_cnt = 2, .start_index = 1, .netif_flag = false,  .help = "ifconfig help: Prints the help text for all ifconfig commands"},
    {.name = "netif",     .operation = ifcfg_netif_op,  .arg_cnt = 4, .start_index = 1, .netif_flag = false,  .help = "ifconfig netif create/destroy <ethernet handle id>/<iface>: Create or destroy a network interface with the specified ethernet handle or interface name"},
    {.name = "eth",       .operation = ifcfg_eth_op,    .arg_cnt = 3, .start_index = 1, .netif_flag = false,  .help = "ifconfig eth init/deinit/show: Initialize, deinitialize and display a list of available ethernet handle"},
    {.name = "ifconfig",  .operation = ifcfg_print_op,  .arg_cnt = 1, .start_index = 0, .netif_flag = false,  .help = "ifconfig: Display a list of all esp_netif interfaces along with their information"},
    {.name = "ifconfig",  .operation = ifcfg_print_op,  .arg_cnt = 2, .start_index = 0, .netif_flag = true,   .help = "ifconfig <iface>: Provide the details of the named interface"},
    {.name = "default",   .operation = ifcfg_basic_op,  .arg_cnt = 3, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> default: Set the specified interface as the default interface"},
    {.name = "ip6",       .operation = ifcfg_basic_op,  .arg_cnt = 3, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> ip6: Enable IPv6 on the specified interface"},
    {.name = "up",        .operation = ifcfg_lwip_op,   .arg_cnt = 3, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> up: Enable the specified interface"},
    {.name = "down",      .operation = ifcfg_lwip_op,   .arg_cnt = 3, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> down: Disable the specified interface"},
    {.name = "link",      .operation = ifcfg_lwip_op,   .arg_cnt = 4, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> link <up/down>: Enable or disable the link of the specified interface"},
    {.name = "napt",      .operation = ifcfg_napt_op,   .arg_cnt = 4, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> napt <enable/disable>: Enable or disable NAPT on the specified interface."},
    {.name = "ip",        .operation = ifcfg_ip_op,     .arg_cnt = 4, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> ip <ipv4 addr>: Set the IPv4 address of the specified interface"},
    {.name = "mask",      .operation = ifcfg_ip_op,     .arg_cnt = 4, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> mask <ipv4 addr>: Set the subnet mask of the specified interface"},
    {.name = "gw",        .operation = ifcfg_ip_op,     .arg_cnt = 4, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> gw <ipv4 addr>: Set the default gateway of the specified interface"},
    {.name = "staticip",  .operation = ifcfg_addr_op,   .arg_cnt = 3, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> staticip: Enables static ip"},
    {.name = "dhcp",      .operation = ifcfg_addr_op,   .arg_cnt = 5, .start_index = 2, .netif_flag = true,   .help = "ifconfig <iface> dhcp server <enable/disable>: Enable or disable the DHCP server.(Note: DHCP server is not supported yet)\n ifconfig <iface> dhcp client <enable/disable>: Enable or disable the DHCP client.\nNote: Disabling the DHCP server and client enables the use of static IP configuration."},
};


esp_err_t ifcfg_help_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);

    for (int i = 0; i < cmd_count; i++) {
        if ((cmd_list[i].help != NULL) && (strlen(cmd_list[i].help) != 0)) {
            printf(" %s\n", cmd_list[i].help);
        }
    }

    return ESP_OK;
}


esp_netif_t *get_esp_netif_from_ifname(char *if_name)
{
    esp_netif_t *esp_netif = NULL;
    esp_err_t ret = ESP_FAIL;
    char interface[10];

    /* Get interface details and own global ipv6 address */
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        esp_netif = esp_netif_next(esp_netif);
        ret = esp_netif_get_netif_impl_name(esp_netif, interface);

        if ((ESP_FAIL == ret) || (NULL == esp_netif)) {
            ESP_LOGE(TAG, "No interface available");
            return NULL;
        }

        if (!strcmp(interface, if_name)) {
            return esp_netif;
        }
    }

    return NULL;
}


esp_err_t ifcfg_basic_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    /* Set Default */
    if (!strcmp("default", argv[self->start_index])) {
        esp_netif_set_default_netif(esp_netif);
        return ESP_OK;
    }

    /* Enable IPv6 on this interface */
    if (!strcmp("ip6", argv[self->start_index])) {
        ESP_ERROR_CHECK(esp_netif_create_ip6_linklocal(esp_netif));
        return ESP_OK;
    }

    return ESP_FAIL;
}


esp_err_t ifcfg_lwip_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    struct netif *lwip_netif = esp_netif_get_netif_impl(esp_netif);
    if (NULL == lwip_netif) {
        ESP_LOGE(TAG, "lwip interface %s not available", argv[1]);
        return ESP_OK;
    }

    /* Enable/Disable Interface */
    if (!strcmp("up", argv[self->start_index])) {
        netif_set_up(lwip_netif);
        return ESP_OK;
    }

    if (!strcmp("down", argv[self->start_index])) {
        netif_set_down(lwip_netif);
        return ESP_OK;
    }

    /* Enable/Disable link */
    if (!strcmp("link", argv[self->start_index])) {

        if (!strcmp("up", argv[self->start_index + 1])) {
            netif_set_link_up(lwip_netif);
        }

        if (!strcmp("down", argv[self->start_index + 1])) {
            netif_set_down(lwip_netif);
            netif_set_link_down(lwip_netif);
        }

        return ESP_OK;
    }

    return ESP_FAIL;
}


esp_err_t ifcfg_ip_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    esp_netif_ip_info_t ip_info = {0};

    esp_netif_dhcpc_stop(esp_netif);
    esp_netif_get_ip_info(esp_netif, &ip_info);

    if (!strcmp("ip", argv[self->start_index])) {
        ESP_LOGI(TAG, "Setting ip: %s", argv[self->start_index + 1]);

        inet_aton(argv[self->start_index + 1], &ip_info.ip.addr);
        esp_netif_set_ip_info(esp_netif, &ip_info);
        return ESP_OK;
    } else if (!strcmp("mask", argv[self->start_index])) {
        ESP_LOGI(TAG, "Setting mask: %s", argv[self->start_index + 1]);

        inet_aton(argv[self->start_index + 1], &ip_info.netmask.addr);
        esp_netif_set_ip_info(esp_netif, &ip_info);
        return ESP_OK;
    } else if (!strcmp("gw", argv[self->start_index])) {
        ESP_LOGI(TAG, "Setting gw: %s", argv[self->start_index + 1]);

        inet_aton(argv[self->start_index + 1], &ip_info.gw.addr);
        esp_netif_set_ip_info(esp_netif, &ip_info);
        return ESP_OK;
    }

    return ESP_FAIL;
}


#if IP_NAPT
esp_err_t set_napt(char *if_name, bool state)
{
    esp_netif_t *esp_netif = NULL;
    esp_err_t ret = ESP_FAIL;
    char interface[10];

    /* Get interface details and own global ipv6 address */
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        esp_netif = esp_netif_next(esp_netif);

        ret = esp_netif_get_netif_impl_name(esp_netif, interface);
        if ((ESP_FAIL == ret) || (NULL == esp_netif)) {
            ESP_LOGE(TAG, "No interface available");
            return ESP_FAIL;
        }

        if (!strcmp(interface, if_name)) {
            struct netif *lwip_netif = esp_netif_get_netif_impl(esp_netif);
            ip_napt_enable_netif(lwip_netif, state);
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}
#endif


esp_err_t ifcfg_napt_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
#if IP_NAPT
    if (!strcmp("napt", argv[self->start_index])) {

        ESP_LOGI(TAG, "Setting napt %s on %s", argv[self->start_index + 1], argv[1]);
        if (!strcmp(argv[self->start_index + 1], "enable")) {
            return set_napt(argv[1], true);
        } else if (!strcmp(argv[self->start_index + 1], "disable")) {
            return set_napt(argv[1], false);
        } else {
            ESP_LOGI(TAG, "Invalid argument: %s", argv[self->start_index + 1]);
        }

        return ESP_FAIL;
    }
#endif
    ESP_LOGE(TAG, "NAPT not enabled in menuconfig");
    return ESP_OK;
}


esp_err_t ifcfg_addr_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    if (!strcmp("staticip", argv[self->start_index])) {
        esp_netif_dhcpc_stop(esp_netif);
        //esp_netif_dhcps_stop(esp_netif);
        return ESP_OK;
    } else if (!strcmp("server", argv[self->start_index + 1])) { // Server
        if (!strcmp("enable", argv[self->start_index + 2])) {
            ESP_LOGW(TAG, "DHCP Server configuration is not supported yet.");    // TBD
            //esp_netif_dhcps_start(esp_netif);
            return ESP_OK;
        } else if (!strcmp("disable", argv[self->start_index + 2])) {
            ESP_LOGW(TAG, "DHCP Server configuration is not supported yet.");    // TBD
            //esp_netif_dhcps_stop(esp_netif);
            return ESP_OK;
        }

        ESP_LOGE(TAG, "Invalid argument");
        return ESP_FAIL;

    } else if (!strcmp("client", argv[self->start_index + 1])) { // Client
        if (!strcmp("enable", argv[self->start_index + 2])) {
            esp_netif_dhcpc_start(esp_netif);
            return ESP_OK;
        } else if (!strcmp("disable", argv[self->start_index + 2])) {
            esp_netif_dhcpc_stop(esp_netif);
            return ESP_OK;
        }

        ESP_LOGE(TAG, "Invalid argument");
        return ESP_FAIL;
    }

    return ESP_FAIL;
}


void print_iface_details(esp_netif_t *esp_netif)
{
    esp_netif_ip_info_t ip_info;
    uint8_t mac[NETIF_MAX_HWADDR_LEN];
    char interface[10];
    int ip6_addrs_count = 0;
    esp_ip6_addr_t ip6[LWIP_IPV6_NUM_ADDRESSES];
    esp_err_t ret = ESP_FAIL;
    esp_netif_dhcp_status_t status;

    struct netif *lwip_netif = esp_netif_get_netif_impl(esp_netif);

    /* Print Interface Name and Number */
    ret = esp_netif_get_netif_impl_name(esp_netif, interface);
    if ((ESP_FAIL == ret) || (NULL == esp_netif)) {
        ESP_LOGE(TAG, "No interface available");
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    if (esp_netif_get_default_netif() == esp_netif) {
        ESP_LOGI(TAG, "Interface Name: %s (DEF)", interface);
    } else {
        ESP_LOGI(TAG, "Interface Name: %s", interface);
    }
#else
    ESP_LOGI(TAG, "Interface Name: %s", interface);
#endif
    ESP_LOGI(TAG, "Interface Number: %d", lwip_netif->num);

    /* Print MAC address */
    esp_netif_get_mac(esp_netif, mac);
    ESP_LOGI(TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
             mac[2], mac[3], mac[4], mac[5]);

    /* Print DHCP status */
    if (ESP_OK == esp_netif_dhcps_get_status(esp_netif, &status)) {
        ESP_LOGI(TAG, "DHCP Server Status: %s", status ? "enabled" : "disabled");
    } else if ((ESP_OK == esp_netif_dhcpc_get_status(esp_netif, &status))) {
        if (ESP_NETIF_DHCP_STOPPED == status) {
            ESP_LOGI(TAG, "Static IP");
        } else {
            ESP_LOGI(TAG, "DHCP Client Status: %s", status ? "enabled" : "disabled");
        }
    }

    /* Print IP Info */
    esp_netif_get_ip_info(esp_netif, &ip_info);
    ESP_LOGI(TAG, "IP: " IPSTR ", MASK: " IPSTR ", GW: " IPSTR, IP2STR(&(ip_info.ip)), IP2STR(&(ip_info.netmask)), IP2STR(&(ip_info.gw)));

#if IP_NAPT
    /* Print NAPT status*/
    ESP_LOGI(TAG, "NAPT: %s", lwip_netif->napt ? "enabled" : "disabled");
#endif

    /* Print IPv6 Address */
    ip6_addrs_count = esp_netif_get_all_ip6(esp_netif, ip6);
    for (int j = 0; j < ip6_addrs_count; ++j) {
        ESP_LOGI(TAG, "IPv6 address: " IPV6STR, IPV62STR(ip6[j]));
    }

    /* Print Interface and Link Status*/
    ESP_LOGI(TAG, "Interface Status: %s", esp_netif_is_netif_up(esp_netif) ? "UP" : "DOWN");
    ESP_LOGI(TAG, "Link Status: %s\n", netif_is_link_up(lwip_netif) ? "UP" : "DOWN");

}


esp_err_t ifcfg_print_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    /* Print interface details */
    if (2 == argc) {
        print_iface_details(esp_netif);
        return ESP_OK;
    }

    /* Get interface details and own global ipv6 address of all interfaces */
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        esp_netif = esp_netif_next(esp_netif);
        print_iface_details(esp_netif);
    }

    return ESP_OK;
}


/* Maximum number of interface that can be added */
#define MAX_ETH_NETIF_COUNT     (10)

typedef enum {
    UNINITIALIZED = 0,
    ETH_INITIALIZED = 1,
    NETIF_CREATED = 2,
    NETIF_DESTROYED = 3,
    ETH_DEINITIALIZED = 4
} iface_state;

typedef struct {
    esp_netif_t *esp_netif;
    esp_eth_handle_t *eth_handle;
    esp_eth_netif_glue_handle_t eth_glue;
    iface_state state;
} iface_desc;

iface_desc iface_list[MAX_ETH_NETIF_COUNT];
uint8_t netif_count;
uint8_t eth_init_flag = false;
uint8_t eth_port_cnt_g = 0;

static esp_err_t get_netif_config(uint16_t id, esp_netif_config_t *eth_cfg_o)
{
    /* Create new default instance of esp-netif for Ethernet */
    char *if_key;
    if (asprintf(&if_key, "IFC_ETH%d", id) == -1) {
        return ESP_FAIL;
    }

    esp_netif_inherent_config_t *esp_eth_base_config = malloc(sizeof(esp_netif_inherent_config_t));
    if (NULL == esp_eth_base_config) {
        return ESP_FAIL;
    }
    *esp_eth_base_config = (esp_netif_inherent_config_t)ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_eth_base_config->if_key = if_key;

    eth_cfg_o->base = esp_eth_base_config;
    eth_cfg_o->driver = NULL;
    eth_cfg_o->stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;

    return ESP_OK;
}


static void free_config(esp_netif_config_t *eth_cfg)
{
    if ((NULL != eth_cfg) && (NULL != eth_cfg->base)) {
        free((void *)(eth_cfg->base->if_key));
        free((void *)(eth_cfg->base));
    }
}


esp_err_t ifcfg_netif_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    int eth_handle_id = atoi(argv[self->start_index + 2]);

    if (!strcmp(argv[self->start_index + 1], "create")) {
        /* Validate ethernet handle */
        if ((eth_handle_id + 1 > eth_port_cnt_g) || (eth_handle_id < 0)) {
            ESP_LOGE(TAG, "Invalid ethernet handle: %s", argv[self->start_index + 2]);
            return ESP_FAIL;
        }

        esp_netif_config_t eth_cfg;
        ESP_ERROR_CHECK(get_netif_config(eth_handle_id, &eth_cfg));
        for (int i = 0; i < MAX_ETH_NETIF_COUNT; i++) {
            if (iface_list[i].state == ETH_INITIALIZED) {
                esp_netif = esp_netif_new(&eth_cfg);
                if (esp_netif == NULL) {
                    ESP_LOGE(TAG, "Interface with key %s already exists", argv[self->start_index + 2]);
                    return ESP_FAIL;
                }

                iface_list[i].eth_glue = esp_eth_new_netif_glue(iface_list[i].eth_handle);
                if (iface_list[i].eth_glue == NULL) {
                    ESP_LOGE(TAG, "%s: eth_glue is NULL", __func__);
                    esp_netif_destroy(esp_netif);
                    return ESP_FAIL;
                }

                iface_list[i].esp_netif = esp_netif;
                ESP_ERROR_CHECK(esp_netif_attach(iface_list[i].esp_netif, iface_list[i].eth_glue));

                // start Ethernet driver state machine
                ESP_ERROR_CHECK(esp_eth_start(iface_list[i].eth_handle));

                free_config(&eth_cfg);
                iface_list[i].state = NETIF_CREATED;
                netif_count++;
                break;
            }
        }

        return ESP_OK;
    } else if (!strcmp(argv[self->start_index + 1], "destroy")) {
        esp_netif = get_esp_netif_from_ifname(argv[self->start_index + 2]);
        if (NULL == esp_netif) {
            ESP_LOGE(TAG, "interface %s not available", argv[1]);
            return ESP_FAIL;
        }

        for (int i = 0; i < MAX_ETH_NETIF_COUNT; i++) {
            if (esp_netif == iface_list[i].esp_netif) {
                if (iface_list[i].state == NETIF_CREATED) {
                    esp_eth_stop(iface_list[i].eth_handle);
                    esp_eth_del_netif_glue(iface_list[i].eth_glue);
                    esp_netif_destroy(iface_list[i].esp_netif);
                    iface_list[i].state = NETIF_DESTROYED;
                    netif_count--;
                    return ESP_OK;
                } else {
                    ESP_LOGE(TAG, "Netif is not in created state");
                    return ESP_FAIL;
                }
                break;
            }
        }

        ESP_LOGE(TAG, "Something is very wrong. Unauthorized Interface.");
        return ESP_FAIL;
    }

    return ESP_FAIL;
}


static void print_eth_info(eth_dev_info_t eth_info, int id)
{
    if (eth_info.type == ETH_DEV_TYPE_INTERNAL_ETH) {
        printf("Internal(%s): pins: %2d,%2d, Id: %d\n", eth_info.name, eth_info.pin.eth_internal_mdc, eth_info.pin.eth_internal_mdio, id);
    } else if (eth_info.type == ETH_DEV_TYPE_SPI) {
        printf("     SPI(%s): pins: %2d,%2d, Id: %d\n", eth_info.name, eth_info.pin.eth_spi_cs, eth_info.pin.eth_spi_int, id);
    } else {
        printf("ethernet handle id(ETH_DEV_TYPE_UNKNOWN): %d\n", id);
    }
}


esp_err_t ifcfg_eth_op(netif_op *self, int argc, char *argv[], esp_netif_t *esp_netif)
{
    static esp_eth_handle_t *eth_handle_g = NULL;
    eth_dev_info_t eth_info;

    if (!strcmp(argv[self->start_index + 1], "init")) {

        /* Check if ethernet is initialized */
        if (eth_init_flag == false) {
            // Initialize Ethernet driver
            if (ethernet_init_all(&eth_handle_g, &eth_port_cnt_g) != ESP_OK) {
                ESP_LOGE(TAG, "Unable to initialize ethernet");
                return ESP_FAIL;
            }
            eth_init_flag = true;

            for (int i = 0; i < eth_port_cnt_g; i++) {
                for (int j = 0; j < MAX_ETH_NETIF_COUNT; j++) {
                    if (iface_list[j].state == UNINITIALIZED) {
                        iface_list[j].eth_handle = eth_handle_g[i];
                        iface_list[j].state = ETH_INITIALIZED;
                        break;
                    }
                }
            }

            if (eth_port_cnt_g > MAX_ETH_NETIF_COUNT) {
                ESP_LOGW(TAG, "Not all ethernet ports can be assigned a network interface.\nPlease reconfigure MAX_ETH_NETIF_COUNT to a higher value.");
            }

        } else {
            ESP_LOGW(TAG, "Ethernet already initialized");
        }

        /* Display available ethernet handles */
        for (int i = 0; i < eth_port_cnt_g; i++) {
            eth_info = ethernet_init_get_dev_info(iface_list[i].eth_handle);
            print_eth_info(eth_info, i);
        }
    } else if (!strcmp(argv[self->start_index + 1], "show")) {
        /* Check if ethernet is initialized */
        if (eth_init_flag == false) {
            // Initialize Ethernet driver
            ESP_LOGE(TAG, "Ethernet is not initialized.");
            return ESP_OK;
        }

        /* Display available ethernet handles */
        for (int i = 0; i < eth_port_cnt_g; i++) {
            eth_info = ethernet_init_get_dev_info(iface_list[i].eth_handle);
            print_eth_info(eth_info, i);
        }
    } else if (!strcmp(argv[self->start_index + 1], "deinit")) {
        /* Check if ethernet is initialized */
        if (eth_init_flag == false) {
            // Initialize Ethernet driver
            ESP_LOGE(TAG, "Ethernet is not initialized.");
            return ESP_OK;
        }

        /* Stop and Deinit ethernet here */
        ethernet_deinit_all(eth_handle_g);
        eth_port_cnt_g = 0;
        eth_init_flag = false;
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}


/* handle 'ifconfig' command */
static int do_cmd_ifconfig(int argc, char **argv)
{
    esp_netif_t *esp_netif = NULL;
    int cmd_count = sizeof(cmd_list) / sizeof(cmd_list[0]);
    netif_op cmd;

    for (int i = 0; i < cmd_count; i++) {
        cmd = cmd_list[i];

        if (argc < cmd.start_index + 1) {
            continue;
        }

        if (!strcmp(cmd.name, argv[cmd.start_index])) {

            /* Get interface for eligible commands */
            if (cmd.netif_flag == true) {
                esp_netif = get_esp_netif_from_ifname(argv[1]);
                if (NULL == esp_netif) {
                    ESP_LOGE(TAG, "interface %s not available", argv[1]);
                    return 0;
                }
            }

            if (cmd.arg_cnt == argc) {
                if (cmd.operation != NULL) {
                    if (cmd.operation(&cmd, argc, argv, esp_netif) != ESP_OK) {
                        ESP_LOGE(TAG, "Usage:\n%s", cmd.help);
                        return 0;
                    }
                }
                return 0;
            }
        }
    }

    ESP_LOGE(TAG, "Command not available");

    return 1;
}


/**
 * @brief Registers the ifconfig command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_ifconfig_register(void)
{
    esp_err_t ret;
    esp_console_cmd_t command = {
        .command = "ifconfig",
        .help = "Command for network interface configuration and monitoring\nFor more info run 'ifconfig help'",
        .func = &do_cmd_ifconfig
    };

    ret = esp_console_cmd_register(&command);
    if (ret) {
        ESP_LOGE(TAG, "Unable to register ifconfig");
    }

    return ret;
}
