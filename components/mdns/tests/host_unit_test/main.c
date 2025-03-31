/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "esp_err.h"
#include "unity.h"
#include "mdns_receive.h"
#include "mdns_responder.h"
#include "mdns_mem_caps.h"
#ifdef ENABLE_UNIT_TESTS
#include "mock_mdns_pcb.h"
#include "mock_mdns_send.h"

void setUp(void)
{
}

void tearDown(void)
{
}

// Sample test case - update based on the actual functionality in mdns_receive.c
void test_mdns_receive_initialization(void)
{
//    mdns_priv_probe_all_pcbs_ExpectAnyArgs();
    // Example of using mocks
    // mock_mdns_pcb_init_ExpectAndReturn(ESP_OK);
    // Add more mock expectations as needed
}

#endif

esp_err_t mdns_packet_push(esp_ip_addr_t *addr, int port, mdns_if_t tcpip_if, uint8_t*data, size_t len);

mdns_search_once_t *s_a, *s_aaaa, *s_ptr, *s_srv, *s_txt;

void init_responder(void)
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

    s_a = mdns_query_async_new("host_name", NULL, NULL, MDNS_TYPE_A, 1000, 1, NULL);
    s_aaaa = mdns_query_async_new("host_name2", NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
    s_ptr = mdns_query_async_new("minifritz", "_http", "_tcp", MDNS_TYPE_PTR, 1000, 1, NULL);
    s_srv = mdns_query_async_new("fritz", "_http", "_tcp", MDNS_TYPE_SRV, 1000, 1, NULL);
    s_txt = mdns_query_async_new("fritz", "_http", "_tcp", MDNS_TYPE_TXT, 1000, 1, NULL);

}

void deinit_responder(void)
{
    mdns_query_async_delete(s_a);
    mdns_query_async_delete(s_aaaa);
    mdns_query_async_delete(s_ptr);
    mdns_query_async_delete(s_srv);
    mdns_query_async_delete(s_txt);
    mdns_service_remove_all();
    mdns_priv_responder_free();
}

static void send_packet(bool ip4, bool mdns_port, uint8_t*data, size_t len)
{
    esp_ip_addr_t addr4 = ESP_IP4ADDR_INIT(192, 168, 1, 1);
    esp_ip_addr_t addr6 = ESP_IP6ADDR_INIT(0x000002ff, 0, 0, 0xfe800000);
    esp_ip_addr_t *addr = ip4 ? &addr4 : &addr6;
    int port = mdns_port ? 53 : 5353;

    if (mdns_packet_push(addr, port, 0, data, len) != ESP_OK) {
        printf("Failed to push packet\n");
    }
}

#ifdef ENABLE_UNIT_TESTS
// Add new test cases for mdns_receive
void test_mdns_receive_from_file(const char* filename)
{
    uint8_t buf[1460];
    FILE *file = fopen(filename, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, "Failed to open test packet file");

    size_t len = fread(buf, 1, 1460, file);
    fclose(file);

    // Test with different packet configurations
    send_packet(true, true, buf, len);
    send_packet(true, false, buf, len);
    send_packet(false, true, buf, len);
    send_packet(false, false, buf, len);

    // Add assertions here based on expected behavior
    // For example:
    // TEST_ASSERT_EQUAL(expected_result, actual_result);
}

void run_unity_tests(int argc, char **argv)
{
    UNITY_BEGIN();

    // Run basic initialization test
    RUN_TEST(test_mdns_receive_initialization);

    // If a packet file is provided as argument, run packet-based tests
    if (argc > 2 && strcmp(argv[1], "--test") == 0) {
        for (int i = 2; i < argc; i++) {
            printf("Testing with packet file: %s\n", argv[i]);
            test_mdns_receive_from_file(argv[i]);
        }
    }

    UNITY_END();
}

void mdns_priv_probe_all_pcbs_Callback(mdns_srv_item_t** services, size_t len, bool probe_ip, bool clear_old_probe, int cmock_num_calls)
{

}

void mdns_priv_create_answer_from_parsed_packet_Callback(mdns_parsed_packet_t* parsed_packet, int cmock_num_calls)
{
    printf("callback\n");
}
#endif

int main(int argc, char **argv)
{
#ifdef ENABLE_UNIT_TESTS
    if (argc >= 2 && strcmp(argv[1], "--test") == 0) {
        mdns_priv_probe_all_pcbs_CMockIgnore();
        mdns_priv_pcb_announce_CMockIgnore();
        mdns_priv_pcb_send_bye_service_CMockIgnore();
        mdns_priv_pcb_check_probing_services_CMockIgnore();
        mdns_priv_pcb_is_after_probing_IgnoreAndReturn(true);

        _mdns_clear_tx_queue_head_CMockIgnore();
        _mdns_remove_scheduled_service_packets_CMockIgnore();
        mdns_priv_create_answer_from_parsed_packet_Stub(mdns_priv_create_answer_from_parsed_packet_Callback);
        //    mdns_priv_probe_all_pcbs_AddCallback(mdns_priv_probe_all_pcbs_Callback);
        //    mdns_priv_probe_all_pcbs_ExpectAnyArgs();

        init_responder();
        run_unity_tests(argc, argv);
        deinit_responder();
        return 0;
    }
#endif

    init_responder();

    // Original fuzzing code
    uint8_t buf[1460];
    FILE *file;
    size_t len = 1460;
    memset(buf, 0, len);
#ifndef __AFL_LOOP
    //
    // Note: parameter1 is a file (mangled packet) which caused the crash
    if (argc != 2) {
        printf("Non-instrumentation mode: please supply a file name created by AFL to reproduce crash\n");
        return 1;
    }
    file = fopen(argv[1], "r");
    assert(file >= 0);
    len = fread(buf, 1, 1460, file);
    fclose(file);
    {
#else
    while (__AFL_LOOP(1000)) {
        memset(buf, 0, 1460);
        size_t len = read(0, buf, 1460);
#endif
        send_packet(true, true, buf, len);
        send_packet(true, false, buf, len);
        send_packet(false, true, buf, len);
        send_packet(false, false, buf, len);
    }
    deinit_responder();

    // ... rest of the existing fuzzing code ...

    return 0;
}
