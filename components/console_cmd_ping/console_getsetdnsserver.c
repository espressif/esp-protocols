/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "sdkconfig.h"
#include "esp_idf_version.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_console.h"
#include "esp_netif.h"
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
    struct arg_str *ifkey;
    struct arg_lit *global;
    struct arg_str *main;
    struct arg_str *backup;
    struct arg_str *fallback;
    struct arg_end *end;
} setdnsserver_args;

/**
 * @brief Structure to hold arguments for the getdnsserver command.
 */
static struct {
    struct arg_str *target;
    struct arg_end *end;
} getdnsserver_args;

static esp_netif_t *netif_next(esp_netif_t *esp_netif)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    return esp_netif_next_unsafe(esp_netif);
#else
    return esp_netif_next(esp_netif);
#endif
}

static esp_netif_t *get_default_netif(void)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    return esp_netif_get_default_netif();
#else
    return NULL;
#endif
}

static void print_available_ifkeys(void)
{
    printf("Available interface keys:\n");
    for (esp_netif_t *esp_netif = netif_next(NULL); esp_netif != NULL; esp_netif = netif_next(esp_netif)) {
        const char *if_key = esp_netif_get_ifkey(esp_netif);
        if (if_key != NULL) {
            printf("  %s\n", if_key);
        }
    }
}

static esp_netif_t *resolve_ifkey(const char *key)
{
    esp_netif_t *esp_netif = esp_netif_get_handle_from_ifkey(key);
    if (esp_netif == NULL) {
        printf("Unknown if_key '%s'\n", key);
        print_available_ifkeys();
    }
    return esp_netif;
}

static void print_one_dns_type(const char *label, const esp_netif_dns_info_t *info)
{
    if (info->ip.type == ESP_IPADDR_TYPE_V4) {
        printf("%s : " IPSTR "\n", label, IP2STR(&info->ip.u_addr.ip4));
    } else if (info->ip.type == ESP_IPADDR_TYPE_V6) {
        printf("%s : " IPV6STR "\n", label, IPV62STR(info->ip.u_addr.ip6));
    } else {
        printf("%s : 0.0.0.0\n", label);
    }
}

static void print_dns_slot(const char *label, esp_netif_t *esp_netif, esp_netif_dns_type_t type)
{
    esp_netif_dns_info_t info = {0};
    esp_err_t ret = esp_netif_get_dns_info(esp_netif, type, &info);
    if (ret != ESP_OK) {
        printf("%s : unavailable (err=0x%x)\n", label, ret);
        return;
    }
    print_one_dns_type(label, &info);
}

static void print_dns_table(esp_netif_t *esp_netif)
{
    if (esp_netif == NULL) {
        esp_netif_dns_info_t info = {0};
        esp_err_t ret = esp_netif_get_dns_info(NULL, ESP_NETIF_DNS_MAIN, &info);
        if (ret != ESP_OK) {
            printf("[global] unavailable (needs CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF=y, IDF >= 5.4)\n");
            return;
        }
        printf("[global]\n");
        print_one_dns_type("Main DNS server", &info);
        print_dns_slot("Backup DNS server", NULL, ESP_NETIF_DNS_BACKUP);
        print_dns_slot("Fallback DNS server", NULL, ESP_NETIF_DNS_FALLBACK);
        return;
    }

    print_dns_slot("Main DNS server", esp_netif, ESP_NETIF_DNS_MAIN);
    print_dns_slot("Backup DNS server", esp_netif, ESP_NETIF_DNS_BACKUP);
    print_dns_slot("Fallback DNS server", esp_netif, ESP_NETIF_DNS_FALLBACK);
}

static void print_netif_header(esp_netif_t *esp_netif, esp_netif_t *default_netif)
{
    char interface[10] = {0};
    esp_err_t ret = esp_netif_get_netif_impl_name(esp_netif, interface);
    if (ret == ESP_OK) {
        printf("Interface Name: %s\n", interface);
    } else {
        printf("Interface Name: ?\n");
    }

    const char *if_key = esp_netif_get_ifkey(esp_netif);
    const bool is_default = (default_netif != NULL && esp_netif == default_netif);
    const bool is_dhcps = (esp_netif_get_flags(esp_netif) & ESP_NETIF_DHCP_SERVER) != 0;

    printf("  if_key: %s%s%s\n",
           if_key ? if_key : "?",
           is_dhcps ? " (dhcps)" : "",
           is_default ? " [default]" : "");
}

static void print_netif_dns(esp_netif_t *esp_netif, esp_netif_t *default_netif)
{
    print_netif_header(esp_netif, default_netif);
    print_dns_table(esp_netif);
}

/**
 * @brief Resolve a DNS server name or address into a dns_info structure.
 *
 * Uses only the first getaddrinfo result.
 */
static esp_err_t fill_dns_from_server(const char *server, esp_netif_dns_info_t *dns)
{
    struct addrinfo hint = {0};
    struct addrinfo *res = NULL;

    int ret = getaddrinfo(server, NULL, &hint, &res);
    if (ret != 0 || res == NULL) {
        printf("setdnsserver: Failure host:%s(ERROR: %d)\n", server, ret);
        ESP_LOGE(TAG, "Failure host");
        return ESP_FAIL;
    }

    memset(dns, 0, sizeof(*dns));
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        dns->ip.u_addr.ip4.addr = ipv4->sin_addr.s_addr;
        dns->ip.type = IPADDR_TYPE_V4;
    } else if (res->ai_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
        memcpy(dns->ip.u_addr.ip6.addr, &ipv6->sin6_addr, sizeof(dns->ip.u_addr.ip6.addr));
        dns->ip.type = IPADDR_TYPE_V6;
    } else {
        ESP_LOGE(TAG, "ai_family Unknown: %d\n", res->ai_family);
        freeaddrinfo(res);
        return ESP_ERR_INVALID_ARG;
    }

    freeaddrinfo(res);
    return ESP_OK;
}

/* dns is not const: esp_netif_set_dns_info() takes a non-const pointer */
static esp_err_t apply_dns_info(esp_netif_t *target, esp_netif_dns_type_t type, esp_netif_dns_info_t *dns)
{
    esp_err_t err = esp_netif_set_dns_info(target, type, dns);
    if (err != ESP_OK) {
        const char *key = (target != NULL) ? esp_netif_get_ifkey(target) : "global";
        printf("setdnsserver: Failed to set DNS on %s (err=0x%x)\n", key ? key : "?", err);
    }
    return err;
}

/**
 * @brief Sets the DNS server address for one target, or for every interface.
 *
 * @param server  IP address or hostname of the DNS server.
 * @param type    Type of the DNS server (main, backup, fallback).
 * @param all_ifaces  If true, apply to every interface (compat path).
 * @param target  Specific netif, or NULL when setting the lwIP global table.
 *
 * @return ESP_OK on success, or an error code if any write failed.
 */
static esp_err_t set_dns_server(const char *server, esp_netif_dns_type_t type, bool all_ifaces, esp_netif_t *target)
{
    esp_netif_dns_info_t dns;
    esp_err_t err = fill_dns_from_server(server, &dns);
    if (err != ESP_OK) {
        return err;
    }

    if (!all_ifaces) {
        return apply_dns_info(target, type, &dns);
    }

    esp_err_t overall = ESP_OK;
    for (esp_netif_t *esp_netif = netif_next(NULL); esp_netif != NULL; esp_netif = netif_next(esp_netif)) {
        if (apply_dns_info(esp_netif, type, &dns) != ESP_OK) {
            overall = ESP_FAIL;
        }
    }
    return overall;
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

    if (setdnsserver_args.ifkey->count > 0 && setdnsserver_args.global->count > 0) {
        printf("setdnsserver: --if and --global are mutually exclusive\n");
        return 1;
    }

    bool all_ifaces = (setdnsserver_args.ifkey->count == 0 && setdnsserver_args.global->count == 0);
    esp_netif_t *target = NULL;
    if (setdnsserver_args.ifkey->count > 0) {
        target = resolve_ifkey(setdnsserver_args.ifkey->sval[0]);
        if (target == NULL) {
            return 1;
        }
    }

    int failed = 0;
    if (set_dns_server(setdnsserver_args.main->sval[0], ESP_NETIF_DNS_MAIN, all_ifaces, target) != ESP_OK) {
        failed = 1;
    }

    if (setdnsserver_args.backup->count > 0) {
        if (set_dns_server(setdnsserver_args.backup->sval[0], ESP_NETIF_DNS_BACKUP, all_ifaces, target) != ESP_OK) {
            failed = 1;
        }
    }

    if (setdnsserver_args.fallback->count > 0) {
        if (set_dns_server(setdnsserver_args.fallback->sval[0], ESP_NETIF_DNS_FALLBACK, all_ifaces, target) != ESP_OK) {
            failed = 1;
        }
    }

    return failed;
}


/**
 * @brief Registers the setdnsserver command.
 *
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t console_cmd_setdnsserver_register(void)
{
    esp_err_t ret;

    /* long-only: -i is --interval and -I is --interface on ping in this same REPL */
    setdnsserver_args.ifkey = arg_str0(NULL, "if", "<if_key>", "Set only this interface (e.g. WIFI_STA_DEF)");
    setdnsserver_args.global = arg_lit0(NULL, "global", "Set lwIP global table (esp_netif=NULL)");
    setdnsserver_args.main = arg_str1(NULL, NULL, "<main>", "The main DNS server IP address.");
    setdnsserver_args.backup = arg_str0(NULL, NULL, "backup", "The secondary DNS server IP address (optional).");
    setdnsserver_args.fallback = arg_str0(NULL, NULL, "fallback", "The fallback DNS server IP address (optional).");
    setdnsserver_args.end = arg_end(3);
    const esp_console_cmd_t setdnsserver_cmd = {
        .command = "setdnsserver",
        .help = "Usage: setdnsserver [--if <if_key>|--global] <main> [backup] [fallback]",
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
 * @brief Command handler for getting DNS server addresses.
 *
 * @param argc Argument count.
 * @param argv Argument values.
 *
 * @return int: 0 on success, 1 on error.
 */
static int do_getdnsserver_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&getdnsserver_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, getdnsserver_args.end, argv[0]);
        return 1;
    }

    const char *filter = (getdnsserver_args.target->count > 0) ? getdnsserver_args.target->sval[0] : NULL;
    esp_netif_t *default_netif = get_default_netif();

    if (filter == NULL) {
        print_dns_table(NULL);
        for (esp_netif_t *esp_netif = netif_next(NULL); esp_netif != NULL; esp_netif = netif_next(esp_netif)) {
            print_netif_dns(esp_netif, default_netif);
        }
        return 0;
    }

    if (strcasecmp(filter, "global") == 0) {
        print_dns_table(NULL);
        return 0;
    }

    esp_netif_t *esp_netif = resolve_ifkey(filter);
    if (esp_netif == NULL) {
        return 1;
    }
    print_netif_dns(esp_netif, default_netif);
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

    getdnsserver_args.target = arg_str0(NULL, NULL, "ifkey|global", "Print only this table: 'global' or an interface key (e.g. WIFI_STA_DEF).");
    getdnsserver_args.end = arg_end(1);
    const esp_console_cmd_t getdnsserver_cmd = {
        .command = "getdnsserver",
        .help = "Usage: getdnsserver [ifkey|global]",
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
