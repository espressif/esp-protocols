/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sys/socket.h" // for INADDR_ANY
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_console.h"
#include "esp_event.h"
#include "esp_bit_defs.h"
#include "argtable3/argtable3.h"
#include "sdkconfig.h"
#include "iperf.h"
#include "console_iperf.h"

/* "iperf" command */

/**
 * Static registration of this plugin is achieved by defining the plugin description
 * structure and placing it into .console_cmd_desc section.
 * The name of the section and its placement is determined by linker.lf file in 'plugins' component.
 */
static const console_cmd_plugin_desc_t __attribute__((section(".console_cmd_desc"), used)) PLUGIN = {
    .name = "console_cmd_iperf",
    .plugin_regd_fn = &console_cmd_iperf_register
};

static const char *TAG = "console_iperf";

static struct {
    struct arg_str *ip;
    struct arg_lit *server;
    struct arg_lit *udp;
    struct arg_lit *version;
    struct arg_int *port;
    struct arg_int *length;
    struct arg_int *interval;
    struct arg_int *time;
    struct arg_int *bw_limit;
    struct arg_lit *abort;
    struct arg_end *end;
} iperf_args;

static int do_cmd_iperf(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&iperf_args);
    /* ethernet iperf only support IPV4 address */
    iperf_cfg_t cfg = {.type = IPERF_IP_TYPE_IPV4};

    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        return 0;
    }

    /* iperf -a */
    if (iperf_args.abort->count != 0) {
        iperf_stop();
        return 0;
    }

    if (((iperf_args.ip->count == 0) && (iperf_args.server->count == 0)) ||
            ((iperf_args.ip->count != 0) && (iperf_args.server->count != 0))) {
        ESP_LOGE(__func__, "Wrong mode! ESP32 should run in client or server mode");
        return 0;
    }

    /* iperf -s */
    if (iperf_args.ip->count == 0) {
        cfg.flag |= IPERF_FLAG_SERVER;
    }
    /* iperf -c SERVER_ADDRESS */
    else {
        cfg.destination_ip4 = esp_ip4addr_aton(iperf_args.ip->sval[0]);
        cfg.flag |= IPERF_FLAG_CLIENT;
    }

    if (iperf_args.length->count == 0) {
        cfg.len_send_buf = 0;
    } else {
        cfg.len_send_buf = iperf_args.length->ival[0];
    }

    cfg.source_ip4 = INADDR_ANY;

    /* iperf -u */
    if (iperf_args.udp->count == 0) {
        cfg.flag |= IPERF_FLAG_TCP;
    } else {
        cfg.flag |= IPERF_FLAG_UDP;
    }

    /* iperf -p */
    if (iperf_args.port->count == 0) {
        cfg.sport = IPERF_DEFAULT_PORT;
        cfg.dport = IPERF_DEFAULT_PORT;
    } else {
        if (cfg.flag & IPERF_FLAG_SERVER) {
            cfg.sport = iperf_args.port->ival[0];
            cfg.dport = IPERF_DEFAULT_PORT;
        } else {
            cfg.sport = IPERF_DEFAULT_PORT;
            cfg.dport = iperf_args.port->ival[0];
        }
    }

    /* iperf -i */
    if (iperf_args.interval->count == 0) {
        cfg.interval = IPERF_DEFAULT_INTERVAL;
    } else {
        cfg.interval = iperf_args.interval->ival[0];
        if (cfg.interval <= 0) {
            cfg.interval = IPERF_DEFAULT_INTERVAL;
        }
    }

    /* iperf -t */
    if (iperf_args.time->count == 0) {
        cfg.time = IPERF_DEFAULT_TIME;
    } else {
        cfg.time = iperf_args.time->ival[0];
        if (cfg.time <= cfg.interval) {
            cfg.time = cfg.interval;
        }
    }

    /* iperf -b */
    if (iperf_args.bw_limit->count == 0) {
        cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
    } else {
        cfg.bw_lim = iperf_args.bw_limit->ival[0];
        if (cfg.bw_lim <= 0) {
            cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
        }
    }

    printf("mode=%s-%s sip=" IPSTR ":%" PRIu16 ", dip=%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ":%" PRIu16 ", interval=%" PRIu32 ", time=%" PRIu32 "\r\n",
           cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
           cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
           (uint16_t) cfg.source_ip4 & 0xFF,
           (uint16_t)(cfg.source_ip4 >> 8) & 0xFF,
           (uint16_t)(cfg.source_ip4 >> 16) & 0xFF,
           (uint16_t)(cfg.source_ip4 >> 24) & 0xFF,
           cfg.sport,
           cfg.destination_ip4 & 0xFF, (cfg.destination_ip4 >> 8) & 0xFF,
           (cfg.destination_ip4 >> 16) & 0xFF, (cfg.destination_ip4 >> 24) & 0xFF, cfg.dport,
           cfg.interval, cfg.time);

    iperf_start(&cfg);
    return 0;
}

/**
 * @brief Registers the iperf command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_iperf_register(void)
{
    iperf_args.ip = arg_str0("c", "client", "<ip>", "run in client mode, connecting to <host>");
    iperf_args.server = arg_lit0("s", "server", "run in server mode");
    iperf_args.udp = arg_lit0("u", "udp", "use UDP rather than TCP");
    iperf_args.version = arg_lit0("V", "ipv6_domain", "use IPV6 address rather than IPV4");
    iperf_args.port = arg_int0("p", "port", "<port>", "server port to listen on/connect to");
    iperf_args.length = arg_int0("l", "len", "<length>", "set read/write buffer size");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "seconds between periodic bandwidth reports");
    iperf_args.time = arg_int0("t", "time", "<time>", "time in seconds to transmit for (default 10 secs)");
    iperf_args.bw_limit = arg_int0("b", "bandwidth", "<bandwidth>", "bandwidth to send at in Mbits/sec");
    iperf_args.abort = arg_lit0("a", "abort", "abort running iperf");
    iperf_args.end = arg_end(1);

    esp_err_t ret;
    const esp_console_cmd_t command = {
        .command = "iperf",
        .help = "Command to measure network performance, through TCP or UDP connections.",
        .hint = NULL,
        .func = &do_cmd_iperf,
        .argtable = &iperf_args
    };

    ret = esp_console_cmd_register(&command);
    if (ret) {
        ESP_LOGE(TAG, "Unable to register iperf");
    }

    return ret;
}
