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
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include <netdb.h>
#include "console_ping.h"


static const char *TAG = "console_getaddrinfo";

#if CONFIG_PING_CMD_AUTO_REGISTRATION
/**
 * @brief  Static registration of the getaddrinfo command plugin.
 *
 * This section registers the plugin description structure and places it into
 * the .console_cmd_desc section, as determined by the linker.lf file in the
 * 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "console_cmd_getddrinfo",
    .plugin_regd_fn = &console_cmd_getaddrinfo_register
};
#endif

/**
 * @brief Structure to hold arguments for the getaddrinfo command.
 */
static struct {
    struct arg_str *family;
    struct arg_str *flags;
    struct arg_str *port_nr;
    struct arg_str *hostname;
    struct arg_end *end;
} getddrinfo_args;

/**
 * @brief Executes the getaddrinfo command.
 *
 * This function parses arguments, sets hints for address resolution, and calls
 * getaddrinfo to resolve the hostname. It then prints the resolved IP addresses
 * and associated information.
 *
 * @param argc Argument count
 * @param argv Argument vector
 *
 * @return int Returns 0 on success, 1 on error.
 */
static int do_getddrinfo_cmd(int argc, char **argv)
{
    char ip_str[INET6_ADDRSTRLEN];
    struct addrinfo hint = {0};
    struct addrinfo *res = NULL, *res_tmp = NULL;
    const char *port_nr_str = NULL;
    int ret = 0;

    int nerrors = arg_parse(argc, argv, (void **)&getddrinfo_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, getddrinfo_args.end, argv[0]);
        return 1;
    }

    /* Set the address family */
    if (getddrinfo_args.family->count > 0) {
        if (strcmp(getddrinfo_args.family->sval[0], "AF_INET") == 0) {
            hint.ai_family = AF_INET;
        } else if (strcmp(getddrinfo_args.family->sval[0], "AF_INET6") == 0) {
            hint.ai_family = AF_INET6;
        } else if (strcmp(getddrinfo_args.family->sval[0], "AF_UNSPEC") == 0) {
            hint.ai_family = AF_UNSPEC;
        } else {
            ESP_LOGE(TAG, "Unknown family");
            return 1;
        }
    }

    /* Set the flags */
    if (getddrinfo_args.flags->count > 0) {
        for (int i = 0; i < getddrinfo_args.flags->count; i++) {
            if (strcmp(getddrinfo_args.flags->sval[i], "AI_PASSIVE") == 0) {
                hint.ai_flags |= AI_PASSIVE;
            } else if (strcmp(getddrinfo_args.flags->sval[i], "AI_CANONNAME") == 0) {
                hint.ai_flags |= AI_CANONNAME;
            } else if (strcmp(getddrinfo_args.flags->sval[i], "AI_NUMERICHOST") == 0) {
                hint.ai_flags |= AI_NUMERICHOST;
            } else if (strcmp(getddrinfo_args.flags->sval[i], "AI_V4MAPPED") == 0) {
                hint.ai_flags |= AI_V4MAPPED;
            } else if (strcmp(getddrinfo_args.flags->sval[i], "AI_ALL") == 0) {
                hint.ai_flags |= AI_ALL;
            } else {
                ESP_LOGE(TAG, "Unknown flag: %s", getddrinfo_args.flags->sval[i]);
                return 1;
            }
        }
    }

    if (getddrinfo_args.port_nr->count > 0) {
        port_nr_str = getddrinfo_args.port_nr->sval[0];
    }

    /* Convert hostname to IP address */
    if (!strcmp(getddrinfo_args.hostname->sval[0], "NULL")) {
        ret = getaddrinfo(NULL, port_nr_str, &hint, &res);
    } else {
        ret = getaddrinfo(getddrinfo_args.hostname->sval[0], port_nr_str, &hint, &res);
    }

    if (ret != 0) {
        printf("getddrinfo: Failure host:%s(ERROR: %d)\n", getddrinfo_args.hostname->sval[0], ret);
        ESP_LOGE(TAG, "Failure host");
        return 1;
    }

    /* Iterate through the results from getaddrinfo */
    for (res_tmp = res; res_tmp != NULL; res_tmp = res_tmp->ai_next) {

        if (res_tmp->ai_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)res_tmp->ai_addr)->sin_addr, ip_str, INET_ADDRSTRLEN);
            printf("\tIP Address: %s\n", ip_str);
            printf("\tAddress Family: AF_INET\n");
        } else if (res_tmp->ai_family == AF_INET6) {
            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res_tmp->ai_addr)->sin6_addr, ip_str, INET6_ADDRSTRLEN);
            printf("\tIP Address: %s\n", ip_str);
            printf("\tAddress Family: AF_INET6\n");
        } else {
            ESP_LOGE(TAG, "ai_family Unknown: %d\n", res_tmp->ai_family);
        }

        /* Print the protocol used */
        printf("\tProtocol: %d\n", res_tmp->ai_protocol);

        /* Print the canonical name if available */
        if (res_tmp->ai_canonname) {
            printf("\tCanonical Name: %s\n", res_tmp->ai_canonname);
        }
    }

    freeaddrinfo(res);

    return 0;
}

/**
 * @brief Registers the getaddrinfo command.
 *
 * @return esp_err_t Returns ESP_OK on success, or an error code on failure.
 */
esp_err_t console_cmd_getaddrinfo_register(void)
{
    esp_err_t ret;

    getddrinfo_args.family = arg_str0("f", "family", "<AF>", "Address family (AF_INET, AF_INET6, AF_UNSPEC).");
    getddrinfo_args.flags = arg_strn("F", "flags", "<FLAGS>", 0, 5, "Special flags (AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST, AI_V4MAPPED, AI_ALL).");
    getddrinfo_args.port_nr = arg_str0("p", "port", "<port>", "String containing a numeric port number.");
    getddrinfo_args.hostname = arg_str1(NULL, NULL, "<hostname>", "Host address");
    getddrinfo_args.end = arg_end(1);
    const esp_console_cmd_t getddrinfo_cmd = {
        .command = "getaddrinfo",
        .help = "Usage: getaddrinfo [options] <hostname> [service]",
        .hint = NULL,
        .func = &do_getddrinfo_cmd,
        .argtable = &getddrinfo_args
    };

    ret = esp_console_cmd_register(&getddrinfo_cmd);
    if (ret) {
        ESP_LOGE(TAG, "Unable to register getddrinfo");
    }

    return ret;
}
