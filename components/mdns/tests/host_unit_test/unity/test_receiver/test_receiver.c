/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "unity.h"
#include "create_test_packet.h"
#include "unity_main.h"
#include "mock_mdns_pcb.h"
#include "mock_mdns_send.h"
#include "mdns_private.h"
#include "mdns_utils.h"

typedef struct {
    size_t count;
    char hostname[MDNS_NAME_BUF_LEN];
} hostname_changed_callback_context_t;

static void hostname_changed_callback(const char *hostname, void *arg)
{
    hostname_changed_callback_context_t *context = arg;

    ++context->count;
    strncpy(context->hostname, hostname, sizeof(context->hostname));
    context->hostname[sizeof(context->hostname) - 1] = '\0';
}

static void test_hostname_changed_callback(void)
{
    hostname_changed_callback_context_t context = { 0 };

    TEST_ASSERT_EQUAL(ESP_OK, mdns_register_hostname_changed_callback(hostname_changed_callback, &context));

    TEST_ASSERT_EQUAL(ESP_OK, mdns_hostname_set("renamed-hostname"));
    TEST_ASSERT_EQUAL_UINT(1, context.count);
    TEST_ASSERT_EQUAL_STRING("renamed-hostname", context.hostname);

    TEST_ASSERT_EQUAL(ESP_OK, mdns_hostname_set("renamed-hostname"));
    TEST_ASSERT_EQUAL_UINT(1, context.count);

    TEST_ASSERT_EQUAL(ESP_OK, mdns_hostname_set("renamed-hostname-again"));
    TEST_ASSERT_EQUAL_UINT(2, context.count);
    TEST_ASSERT_EQUAL_STRING("renamed-hostname-again", context.hostname);
}

static void test_mdns_hostname_queries(void)
{
    // Define the queries for test4.local and test.local
    mdns_test_query_t queries[] = {
        { "test4.local", 1, 1 },  // A record for test4.local
        { "test.local", 1, 1 }    // A record for test.local
    };

    // Create and send the packet
    size_t packet_len;
    uint8_t* packet = create_mdns_test_packet(
                          queries, 2,    // Queries
                          NULL, 0,       // No answers
                          NULL, 0,       // No additional records
                          &packet_len
                      );

    send_test_packet_multiple(packet, packet_len);
}

// Example of a more complex test with answers and additional records
static void test_mdns_with_answers(void)
{
    // Define a query for _http._tcp.local PTR record
    mdns_test_query_t queries[] = {
        { "_http._tcp.local", 12, 1 }  // PTR record
    };

    // Example data for a PTR record (simplified)
    uint8_t ptr_data[200];
    size_t ptr_data_len = encode_dns_name(ptr_data, "test._http._tcp.local");

    // Define an answer for the PTR record
    mdns_test_answer_t answers[] = {
        { "_http._tcp.local", 12, 1, 120, ptr_data_len, ptr_data }
    };

    // Create and send the packet
    size_t packet_len;
    uint8_t* packet = create_mdns_test_packet(
                          queries, 1,    // Single query
                          answers, 1,    // Single answer
                          NULL, 0,       // No additional records
                          &packet_len
                      );

    send_test_packet_multiple(packet, packet_len);

}

/*
 * Regression test for fa84ee6: packets shorter than MDNS_HEAD_LEN (12) must
 * be rejected before reading the additional RR count at offset 10.
 * An 11-byte AFL input (id_000005) previously passed the old check and read
 * one byte past the RX buffer.
 */
static void test_mdns_reject_short_packet(void)
{
    static const uint8_t eleven_byte_packet[] = {
        0x00, 0xc0, 0x32, 0x00, 0x01, 0x00, 0x14, 0x00, 0x00, 0x54, 0x01
    };

    for (size_t len = 0; len < MDNS_HEAD_LEN; len++) {
        const uint8_t *data = eleven_byte_packet;
        send_packet(true, true, (uint8_t *)data, len);
        send_packet(true, false, (uint8_t *)data, len);
        send_packet(false, true, (uint8_t *)data, len);
        send_packet(false, false, (uint8_t *)data, len);
    }
}

static void test_mdns_subtype_any_question_is_not_a_probe(void)
{
    mdns_test_query_t queries[] = {
        { "subtype._sub._http._tcp.local", MDNS_TYPE_ANY, 1 },
    };
    size_t packet_len;
    uint8_t *packet = create_mdns_test_packet(queries, 1, NULL, 0, NULL, 0, &packet_len);

    send_test_packet_multiple(packet, packet_len);
}

static void test_mdns_reject_misplaced_sub_label(void)
{
    uint8_t fqdn[MDNS_NAME_BUF_LEN] = {};
    mdns_name_t name = {};
    size_t fqdn_len = encode_dns_name(fqdn, "instance._http._sub._tcp.local");

    TEST_ASSERT_NOT_NULL(mdns_utils_parse_fqdn(fqdn, fqdn, &name, fqdn_len));
    TEST_ASSERT_TRUE(name.invalid);

    fqdn_len = encode_dns_name(fqdn, "subtype._sub._http._tcp.local");
    TEST_ASSERT_NOT_NULL(mdns_utils_parse_fqdn(fqdn, fqdn, &name, fqdn_len));
    TEST_ASSERT_FALSE(name.invalid);
    TEST_ASSERT_TRUE(name.sub);
}

static void mdns_priv_create_answer_from_parsed_packet_Callback(mdns_parsed_packet_t* parsed_packet, int cmock_num_calls)
{
    printf("callback\n");
    if (parsed_packet->questions && parsed_packet->questions->sub && parsed_packet->questions->type == MDNS_TYPE_ANY) {
        TEST_ASSERT_FALSE(parsed_packet->probe);
    }
}

void setup_cmock(void)
{
    mdns_priv_probe_all_pcbs_CMockIgnore();
    mdns_priv_pcb_announce_CMockIgnore();
    mdns_priv_pcb_send_bye_service_CMockIgnore();
    mdns_priv_pcb_check_probing_services_CMockIgnore();
    mdns_priv_pcb_is_after_probing_IgnoreAndReturn(true);

    mdns_priv_clear_tx_queue_CMockIgnore();
    mdns_priv_remove_scheduled_service_packets_CMockIgnore();
    mdns_priv_create_answer_from_parsed_packet_Stub(mdns_priv_create_answer_from_parsed_packet_Callback);
}

void run_unity_tests(void)
{
    UNITY_BEGIN();

    // Run hostname queries test
    RUN_TEST(test_mdns_hostname_queries);

    RUN_TEST(test_hostname_changed_callback);

    // Run test with answers
    RUN_TEST(test_mdns_with_answers);

    RUN_TEST(test_mdns_reject_short_packet);

    RUN_TEST(test_mdns_subtype_any_question_is_not_a_probe);

    RUN_TEST(test_mdns_reject_misplaced_sub_label);

    UNITY_END();
}
