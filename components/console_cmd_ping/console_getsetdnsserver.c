/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_console.h"
#include "esp_netif.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include <netdb.h>
#include "console_ping.h"


static const char *TAG = "console_setdnsserver";

#if CONFIG_PING_CMD_AUTO_REGISTRATION
static esp_err_t console_cmd_dnscmd_register(void);

/**
 * @brief  Static registration of the getaddrinfo command plugin.
 *
 * This section registers the plugin description structure and places it into
 * the .console_cmd_desc section, as determined by the linker.lf file in the
 * 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "console_cmd_dnscmd",
    .plugin_regd_fn = &console_cmd_dnscmd_register
};


/**
 * @brief Registers the DNS commands (setdnsserver and getdnsserver) with the console.
 *
 * @return esp_err_t Returns ESP_OK.
 */
static esp_err_t console_cmd_dnscmd_register(void)
{
    console_cmd_setdnsserver_register();
    console_cmd_getdnsserver_register();

    return ESP_OK;
}
#endif

/**
 * @brief Structure to hold arguments for the setdnsserver command.
 */
static struct {
    struct arg_str *main;
    struct arg_str *backup;
    struct arg_str *fallback;
    struct arg_end *end;
} setdnsserver_args;

/**
 * @brief Sets the DNS server address for all network interfaces.
 *
 * This function iterates over all network interfaces available on the ESP32 device
 * and sets the DNS server address for the specified DNS type (main, backup, or fallback).
 * The DNS address is only set if a valid address is provided (non-zero and not equal to IPADDR_NONE).
 *
 * @param server IP address of the DNS server.
 * @param type Type of the DNS server (main, backup, fallback).
 *
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
static esp_err_t set_dns_server(const char *server, esp_netif_dns_type_t type)
{
    int ret = 0;
    struct addrinfo hint = {0};
    struct addrinfo *res = NULL, *res_tmp = NULL;
    esp_netif_t *esp_netif = NULL;
    esp_netif_dns_info_t dns;
    int addr_cnt = 0;

    ret = getaddrinfo(server, NULL, &hint, &res);
    if (ret != 0) {
        printf("setdnsserver: Failure host:%s(ERROR: %d)\n", server, ret);
        ESP_LOGE(TAG, "Failure host");
        return 1;
    }

    for (res_tmp = res; res_tmp != NULL; res_tmp = res_tmp->ai_next) {

        if (addr_cnt == 0) {
            if (res_tmp->ai_family == AF_INET) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
                while ((esp_netif = esp_netif_next_unsafe(esp_netif)) != NULL) {
#else
                while ((esp_netif = esp_netif_next(esp_netif)) != NULL) {
#endif
                    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res_tmp->ai_addr;
                    dns.ip.u_addr.ip4.addr = ipv4->sin_addr.s_addr;
                    dns.ip.type = IPADDR_TYPE_V4;
                    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif, type, &dns));
                }
            } else if (res_tmp->ai_family == AF_INET6) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
                while ((esp_netif = esp_netif_next_unsafe(esp_netif)) != NULL) {
#else
                while ((esp_netif = esp_netif_next(esp_netif)) != NULL) {
#endif
                    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res_tmp->ai_addr;
                    memcpy(dns.ip.u_addr.ip6.addr, &ipv6->sin6_addr, sizeof(dns.ip.u_addr.ip6.addr));
                    dns.ip.type = IPADDR_TYPE_V6;
                    ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif, type, &dns));
                }
            } else {
                ESP_LOGE(TAG, "ai_family Unknown: %d\n", res_tmp->ai_family);
            }
        }
        addr_cnt++;
    }

    freeaddrinfo(res);

    return ESP_OK;
}

/**
 * @brief Command handler for setting DNS server addresses.
 *
 * @param argc Argument count.
 * @param argv Argument values.
 *
 * @return int: 0 on success, 1 on error.
 */
static int do_setdnsserver_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&setdnsserver_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, setdnsserver_args.end, argv[0]);
        return 1;
    }

    set_dns_server(setdnsserver_args.main->sval[0], ESP_NETIF_DNS_MAIN);

    if (setdnsserver_args.backup->count > 0) {
        set_dns_server(setdnsserver_args.backup->sval[0], ESP_NETIF_DNS_BACKUP);
    }

    if (setdnsserver_args.fallback->count > 0) {
        set_dns_server(setdnsserver_args.fallback->sval[0], ESP_NETIF_DNS_FALLBACK);
    }

    return 0;
}


/**
 * @brief Registers the setdnsserver command.
 *
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t console_cmd_setdnsserver_register(void)
{
    esp_err_t ret;

    setdnsserver_args.main = arg_str1(NULL, NULL, "<main>", "The main DNS server IP address.");
    setdnsserver_args.backup = arg_str0(NULL, NULL, "backup", "The secondary DNS server IP address (optional).");
    setdnsserver_args.fallback = arg_str0(NULL, NULL, "fallback", "The fallback DNS server IP address (optional).");
    setdnsserver_args.end = arg_end(1);
    const esp_console_cmd_t setdnsserver_cmd = {
        .command = "setdnsserver",
        .help = "Usage: setdnsserver <main> [backup] [fallback]",
        .hint = NULL,
        .func = &do_setdnsserver_cmd,
        .argtable = &setdnsserver_args
    };

    ret = esp_console_cmd_register(&setdnsserver_cmd);
    if (ret) {
        ESP_LOGE(TAG, "Unable to register setdnsserver");
    }

    return ret;
}


/**
 * @brief Structure to hold arguments for the getdnsserver command.
 */
static struct {
    struct arg_end *end;
} getdnsserver_args;

/**
 * @brief Command handler for getting DNS server addresses.
 *
 * @param argc Argument count.
 * @param argv Argument values.
 *
 * @return int: 0 on success, 1 on error.
 */
static int do_getdnsserver_cmd(int argc, char **argv)
{
    esp_netif_t *esp_netif = NULL;
    esp_netif_dns_info_t dns_info;
    char interface[10];
    esp_err_t ret = ESP_FAIL;

    int nerrors = arg_parse(argc, argv, (void **)&getdnsserver_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, getdnsserver_args.end, argv[0]);
        return 1;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    while ((esp_netif = esp_netif_next_unsafe(esp_netif)) != NULL) {
#else
    while ((esp_netif = esp_netif_next(esp_netif)) != NULL) {
#endif

        /* Print Interface Name and Number */
        ret = esp_netif_get_netif_impl_name(esp_netif, interface);
        if ((ESP_FAIL == ret) || (NULL == esp_netif)) {
            ESP_LOGE(TAG, "No interface available");
            return 1;
        }

        printf("Interface Name: %s\n", interface);
        ESP_ERROR_CHECK(esp_netif_get_dns_info(esp_netif, ESP_NETIF_DNS_MAIN, &dns_info));
        if (dns_info.ip.type == ESP_IPADDR_TYPE_V4) {
            printf("Main DNS server : " IPSTR "\n", IP2STR(&dns_info.ip.u_addr.ip4));
        } else if (dns_info.ip.type == ESP_IPADDR_TYPE_V6) {
            printf("Main DNS server : " IPV6STR "\n", IPV62STR(dns_info.ip.u_addr.ip6));
        }

        ESP_ERROR_CHECK(esp_netif_get_dns_info(esp_netif, ESP_NETIF_DNS_BACKUP, &dns_info));
        if (dns_info.ip.type == ESP_IPADDR_TYPE_V4) {
            printf("Backup DNS server : " IPSTR "\n", IP2STR(&dns_info.ip.u_addr.ip4));
        } else if (dns_info.ip.type == ESP_IPADDR_TYPE_V6) {
            printf("Backup DNS server : " IPV6STR "\n", IPV62STR(dns_info.ip.u_addr.ip6));
        }

        ESP_ERROR_CHECK(esp_netif_get_dns_info(esp_netif, ESP_NETIF_DNS_FALLBACK, &dns_info));
        if (dns_info.ip.type == ESP_IPADDR_TYPE_V4) {
            printf("Fallback DNS server : " IPSTR "\n", IP2STR(&dns_info.ip.u_addr.ip4));
        } else if (dns_info.ip.type == ESP_IPADDR_TYPE_V6) {
            printf("Fallback DNS server : " IPV6STR "\n", IPV62STR(dns_info.ip.u_addr.ip6));
        }
    }

    return 0;
}

/**
 * @brief Registers the getdnsserver command.
 *
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t console_cmd_getdnsserver_register(void)
{
    esp_err_t ret;

    getdnsserver_args.end = arg_end(1);
    const esp_console_cmd_t getdnsserver_cmd = {
        .command = "getdnsserver",
        .help = "Usage: getdnsserver",
        .hint = NULL,
        .func = &do_getdnsserver_cmd,
        .argtable = &getdnsserver_args
    };

    ret = esp_console_cmd_register(&getdnsserver_cmd);
    if (ret) {
        ESP_LOGE(TAG, "Unable to register getdnsserver");
    }

    return ret;
}
