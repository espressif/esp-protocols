/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * AFL++ fuzz harness for the mDNS receive path.
 *
 * Build with -DFUZZ_TARGET=receive (default) or -DFUZZ_TARGET=browse.
 * See fuzzing.md for how this maps to AFL++ effectiveness tips.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "esp_err.h"
#include "mdns.h"
#include "mdns_private.h"
#include "mdns_receive.h"
#include "mdns_responder.h"
#include "mdns_querier.h"
#include "mdns_browser.h"
#include "mdns_cache.h"
#include "mdns_mem_caps.h"

/* Keep AFL, legacy stdin, and crash-repro paths on the same length bound. */
#define FUZZ_MAX_INPUT_LEN MDNS_MAX_PACKET_SIZE

#ifndef FUZZ_TARGET_BROWSE
#define FUZZ_TARGET_RECEIVE 1
#endif

esp_err_t mdns_packet_push(esp_ip_addr_t *addr, int port, mdns_if_t tcpip_if, uint8_t *data, size_t len);

#ifdef FUZZ_TARGET_RECEIVE
static mdns_search_once_t *s_a, *s_aaaa, *s_ptr, *s_srv, *s_txt;
#endif

#ifdef FUZZ_TARGET_BROWSE
static void browse_notifier(mdns_result_t *result)
{
    (void)result;
}
#endif

/*
 * Shared services / hostname setup used by both fuzz targets.
 * Keep this lean: more state means more cross-test interference.
 */
static void init_common(void)
{
    mdns_ip_addr_t addr = { .addr = { .u_addr = ESP_IPADDR_TYPE_V4 } };
    addr.addr.u_addr.ip4.addr = 0x11111111;
    mdns_txt_item_t txt[4] = {
        {"board", "esp32"},
        {"tcp_check", "no"},
        {"ssh_upload", "no"},
        {"auth_upload", "no"}
    };
    mdns_priv_responder_init();
    mdns_hostname_set("test");
    mdns_instance_name_set("test2");
    mdns_delegate_hostname_add("test3", NULL);
    mdns_delegate_hostname_add("test4", &addr);
    mdns_service_add("inst1", "_http", "_tcp", 80, txt, 4);
    mdns_service_subtype_add_for_host("inst1", "_http", "_tcp", "test", "subtype");
    mdns_service_add("inst2", "_http", "_tcp", 80, txt, 1);
    mdns_service_subtype_add_for_host("inst2", "_http", "_tcp", "test", "subtype3");
    mdns_service_add("inst3", "_http", "_tcp", 80, NULL, 0);
    mdns_service_add_for_host("deleg1", "_http", "_tcp", "test3", 80, txt, 2);
    mdns_service_add_for_host(NULL, "_http", "_tcp", "test4", 80, txt, 2);
    mdns_service_add(NULL, "_scanner", "_tcp", 80, NULL, 0);
    mdns_service_add("inst5", "_scanner", "_tcp", 80, NULL, 0);
    mdns_service_add("inst6", "_http", "_tcp", 80, NULL, 0);
    mdns_service_add("inst7", "_sleep", "_udp", 80, NULL, 0);
}

#ifdef FUZZ_TARGET_BROWSE
static void init_fuzz_context(void)
{
    init_common();
    /* Browse path: active browsers so PTR/SRV/TXT answers hit cache + TXT compare. */
    mdns_browse_new("_http", "_tcp", browse_notifier);
    mdns_browse_new("_scanner", "_tcp", browse_notifier);
    mdns_browse_new("_sleep", "_udp", browse_notifier);
}

static void deinit_fuzz_context(void)
{
    mdns_priv_browse_free();
    mdns_priv_cache_clear();
    mdns_service_remove_all();
    mdns_priv_responder_free();
}
#else /* FUZZ_TARGET_RECEIVE */
static void init_fuzz_context(void)
{
    init_common();
    /* Receive/parse path: outstanding queries exercise answer matching without browse. */
    s_a = mdns_query_async_new("host_name", NULL, NULL, MDNS_TYPE_A, 1000, 1, NULL);
    s_aaaa = mdns_query_async_new("host_name2", NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
    s_ptr = mdns_query_async_new("minifritz", "_http", "_tcp", MDNS_TYPE_PTR, 1000, 1, NULL);
    s_srv = mdns_query_async_new("fritz", "_http", "_tcp", MDNS_TYPE_SRV, 1000, 1, NULL);
    s_txt = mdns_query_async_new("fritz", "_http", "_tcp", MDNS_TYPE_TXT, 1000, 1, NULL);
}

static void deinit_fuzz_context(void)
{
    mdns_priv_search_once_free(s_a);
    mdns_priv_search_once_free(s_aaaa);
    mdns_priv_search_once_free(s_ptr);
    mdns_priv_search_once_free(s_srv);
    mdns_priv_search_once_free(s_txt);
    mdns_priv_query_free();
    mdns_priv_cache_clear();
    mdns_service_remove_all();
    mdns_priv_responder_free();
    s_a = NULL;
    s_aaaa = NULL;
    s_ptr = NULL;
    s_srv = NULL;
    s_txt = NULL;
}
#endif

/*
 * Feed one sized packet into the receive path.
 * Derive IPv4/IPv6 and port 53/5353 from the input so each AFL execution
 * explores one transport combo without mutating/stripping the DNS bytes
 * (seed corpus from generate_cases.py stays valid).
 */
static void send_packet(const uint8_t *data, size_t len)
{
    esp_ip_addr_t addr4 = ESP_IP4ADDR_INIT(192, 168, 1, 1);
    esp_ip_addr_t addr6 = ESP_IP6ADDR_INIT(0x000002ff, 0, 0, 0xfe800000);
    uint8_t sel = 0;
    for (size_t i = 0; i < len; i++) {
        sel ^= data[i];
    }
    bool ip4 = (sel & 1u) == 0;
    bool mdns_port = (sel & 2u) == 0;
    esp_ip_addr_t *addr = ip4 ? &addr4 : &addr6;
    int port = mdns_port ? 5353 : 53;

    (void)mdns_packet_push(addr, port, 0, (uint8_t *)data, len);
}

/*
 * Reset mutable global state between persistent-mode iterations so one input
 * cannot poison the next (cache entries from previous packets).
 */
static void reset_between_inputs(void)
{
    mdns_priv_cache_clear();
}

/*
 * Copy at most FUZZ_MAX_INPUT_LEN bytes into *input_copy and feed the receive path.
 * Caps SHM / file lengths so fuzzing and crash reproduction stay aligned.
 */
static void process_fuzz_input(uint8_t **input_copy, const uint8_t *src, size_t len)
{
    if (len > FUZZ_MAX_INPUT_LEN) {
        len = FUZZ_MAX_INPUT_LEN;
    }
    *input_copy = realloc(*input_copy, len ? len : 1);
    if (!*input_copy) {
        return;
    }
    if (len > 0) {
        memcpy(*input_copy, src, len);
    }
    send_packet(*input_copy, len);
    reset_between_inputs();
}

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

int main(int argc, char **argv)
{
    uint8_t *input_copy = NULL;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    init_fuzz_context();

#ifdef __AFL_LOOP
#ifdef __AFL_HAVE_MANUAL_CONTROL
    {
        unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

        while (__AFL_LOOP(10000)) {
            int len = __AFL_FUZZ_TESTCASE_LEN;
            if (len < 0) {
                continue;
            }
            process_fuzz_input(&input_copy, buf, (size_t)len);
        }
    }
#else
    {
        /* Persistent mode without deferred SHM (legacy afl-gcc style). */
        unsigned char stack_buf[FUZZ_MAX_INPUT_LEN];

        while (__AFL_LOOP(10000)) {
            memset(stack_buf, 0, sizeof(stack_buf));
            int len = (int)read(0, stack_buf, sizeof(stack_buf));
            if (len < 0) {
                continue;
            }
            process_fuzz_input(&input_copy, stack_buf, (size_t)len);
        }
    }
#endif /* __AFL_HAVE_MANUAL_CONTROL */
#else
    /* Crash reproduction: pass an AFL crash file on the command line. */
    uint8_t file_buf[FUZZ_MAX_INPUT_LEN];
    size_t len;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <crash-file>\n", argv[0]);
        return 1;
    }
    FILE *file = fopen(argv[1], "rb");
    assert(file != NULL);
    len = fread(file_buf, 1, sizeof(file_buf), file);
    if (!feof(file)) {
        fprintf(stderr, "warning: crash file larger than FUZZ_MAX_INPUT_LEN (%d); truncating\n",
                FUZZ_MAX_INPUT_LEN);
    }
    fclose(file);

    process_fuzz_input(&input_copy, file_buf, len);
    free(input_copy);
#endif

    deinit_fuzz_context();
    return 0;
}
