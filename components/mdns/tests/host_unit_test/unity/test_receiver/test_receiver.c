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
#include "mdns_responder.h"
#include "mdns_netif.h"
#include "mdns_cache.h"

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

static size_t count_registered_services(void)
{
    size_t count = 0;
    for (mdns_srv_item_t *a = mdns_priv_get_services(); a; a = a->next) {
        count++;
    }
    return count;
}

static size_t s_parsed_question_count;
static size_t s_parsed_sdptr_count;

static void discovery_dedup_answer_callback(mdns_parsed_packet_t *parsed_packet, int cmock_num_calls)
{
    (void)cmock_num_calls;
    s_parsed_question_count = 0;
    s_parsed_sdptr_count = 0;
    TEST_ASSERT_TRUE(parsed_packet->discovery);
    for (mdns_parsed_question_t *q = parsed_packet->questions; q; q = q->next) {
        s_parsed_question_count++;
        if (q->type == MDNS_TYPE_SDPTR) {
            s_parsed_sdptr_count++;
        }
    }
}

/*
 * Duplicate _services._dns-sd._udp.local PTR questions must expand the service
 * list only once. Without the guard, each copy allocates one SDPTR question per
 * registered service (heap amplification).
 */
static void test_mdns_discovery_questions_deduped(void)
{
    size_t service_count = count_registered_services();
    TEST_ASSERT_GREATER_THAN(0, service_count);

    mdns_test_query_t queries[] = {
        { "_services._dns-sd._udp.local", MDNS_TYPE_PTR, 1 },
        { "_services._dns-sd._udp.local", MDNS_TYPE_PTR, 1 },
        { "_services._dns-sd._udp.local", MDNS_TYPE_PTR, 1 },
        { "_services._dns-sd._udp.local", MDNS_TYPE_PTR, 1 },
    };

    mdns_priv_create_answer_from_parsed_packet_Stub(discovery_dedup_answer_callback);

    size_t packet_len;
    uint8_t *packet = create_mdns_test_packet(queries, 4, NULL, 0, NULL, 0, &packet_len);
    TEST_ASSERT_NOT_NULL(packet);
    send_packet(true, true, packet, packet_len);
    free(packet);

    TEST_ASSERT_EQUAL(service_count, s_parsed_sdptr_count);
    TEST_ASSERT_EQUAL(service_count, s_parsed_question_count);

    mdns_priv_create_answer_from_parsed_packet_Stub(mdns_priv_create_answer_from_parsed_packet_Callback);
}

// Index that the esp_netif_get_netif_impl_index() stub returns for a netif
#define ZONE_TEST_NETIF_INDEX 2

static uint8_t s_zone_netif_storage;
static uint8_t s_link_local_ip6[16] = { 0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x11, 0x22, 0xff, 0xfe, 0x33, 0x44, 0x55 };
static uint8_t s_global_ip6[16] = { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01 };
static size_t s_zone_browse_calls;
static int s_link_local_zone;
static int s_global_zone;

static void record_ip6_zones(const mdns_ip_addr_t *addr_list)
{
    for (const mdns_ip_addr_t *a = addr_list; a; a = a->next) {
        if (a->addr.type != ESP_IPADDR_TYPE_V6) {
            continue;
        }
        if (!memcmp(a->addr.u_addr.ip6.addr, s_link_local_ip6, sizeof(s_link_local_ip6))) {
            s_link_local_zone = a->addr.u_addr.ip6.zone;
        } else if (!memcmp(a->addr.u_addr.ip6.addr, s_global_ip6, sizeof(s_global_ip6))) {
            s_global_zone = a->addr.u_addr.ip6.zone;
        }
    }
}

static void zone_browse_callback(mdns_result_t *result)
{
    s_zone_browse_calls++;
    record_ip6_zones(result->addr);
}

/*
 * Received IPv6 addresses must have a defined zone: a link-local address gets
 * the index of the receiving interface, and a global address gets zone 0.
 */
static void test_mdns_aaaa_zone(void)
{
    esp_netif_t *netif = (esp_netif_t *)&s_zone_netif_storage;
    s_zone_browse_calls = 0;
    s_link_local_zone = -1;
    s_global_zone = -1;

    // send_packet() delivers packets on interface 0
    TEST_ASSERT_EQUAL(ESP_OK, mdns_register_netif(netif));
    TEST_ASSERT_EQUAL_PTR(netif, mdns_priv_get_esp_netif(0));
    TEST_ASSERT_NOT_NULL(mdns_browse_new("_zone", "_tcp", zone_browse_callback));

    uint8_t ptr_data[MDNS_NAME_BUF_LEN];
    size_t ptr_data_len = encode_dns_name(ptr_data, "zone-inst._zone._tcp.local");
    uint8_t srv_data[MDNS_SRV_FQDN_OFFSET + MDNS_NAME_BUF_LEN] = { 0, 0, 0, 0, 0, 80 }; // priority, weight, port
    size_t srv_data_len = MDNS_SRV_FQDN_OFFSET + encode_dns_name(srv_data + MDNS_SRV_FQDN_OFFSET, "zone-host.local");

    mdns_test_answer_t answers[] = {
        { "_zone._tcp.local", MDNS_TYPE_PTR, 1, 120, ptr_data_len, ptr_data },
        { "zone-inst._zone._tcp.local", MDNS_TYPE_SRV, 1, 120, srv_data_len, srv_data },
    };
    mdns_test_answer_t additional[] = {
        { "zone-host.local", MDNS_TYPE_AAAA, 1, 120, sizeof(s_link_local_ip6), s_link_local_ip6 },
        { "zone-host.local", MDNS_TYPE_AAAA, 1, 120, sizeof(s_global_ip6), s_global_ip6 },
    };

    size_t packet_len;
    uint8_t *packet = create_mdns_test_packet(NULL, 0, answers, 2, additional, 2, &packet_len);
    TEST_ASSERT_NOT_NULL(packet);
    send_packet(true, false, packet, packet_len);
    free(packet);
    mdns_priv_cache_process_sync();

    TEST_ASSERT_GREATER_THAN(0, s_zone_browse_calls);
    TEST_ASSERT_EQUAL_INT(ZONE_TEST_NETIF_INDEX, s_link_local_zone);
    TEST_ASSERT_EQUAL_INT(0, s_global_zone);

    TEST_ASSERT_EQUAL(ESP_OK, mdns_browse_delete("_zone", "_tcp"));
    mdns_priv_cache_clear();
    TEST_ASSERT_EQUAL(ESP_OK, mdns_unregister_netif(netif));
}

/*
 * Query results carry the zone of a received IPv6 address in the same way as browse results.
 */
static void test_mdns_aaaa_zone_in_query_results(void)
{
    esp_netif_t *netif = (esp_netif_t *)&s_zone_netif_storage;
    s_link_local_zone = -1;
    s_global_zone = -1;

    // send_packet() delivers packets on interface 0
    TEST_ASSERT_EQUAL(ESP_OK, mdns_register_netif(netif));
    mdns_search_once_t *search = mdns_query_async_new("zone-query-host", NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
    TEST_ASSERT_NOT_NULL(search);

    mdns_test_answer_t answers[] = {
        { "zone-query-host.local", MDNS_TYPE_AAAA, 1, 120, sizeof(s_link_local_ip6), s_link_local_ip6 },
        { "zone-query-host.local", MDNS_TYPE_AAAA, 1, 120, sizeof(s_global_ip6), s_global_ip6 },
    };

    size_t packet_len;
    uint8_t *packet = create_mdns_test_packet(NULL, 0, answers, 2, NULL, 0, &packet_len);
    TEST_ASSERT_NOT_NULL(packet);
    send_packet(true, false, packet, packet_len);
    free(packet);

    mdns_result_t *results = NULL;
    uint8_t num_results = 0;
    TEST_ASSERT_TRUE(mdns_query_async_get_results(search, 0, &results, &num_results));
    TEST_ASSERT_EQUAL_UINT8(1, num_results);
    TEST_ASSERT_NOT_NULL(results);
    record_ip6_zones(results->addr);
    mdns_query_results_free(results);
    TEST_ASSERT_EQUAL(ESP_OK, mdns_query_async_delete(search));
    TEST_ASSERT_EQUAL(ESP_OK, mdns_unregister_netif(netif));

    TEST_ASSERT_EQUAL_INT(ZONE_TEST_NETIF_INDEX, s_link_local_zone);
    TEST_ASSERT_EQUAL_INT(0, s_global_zone);
}

void setup_cmock(void)
{
    mdns_priv_probe_all_pcbs_CMockIgnore();
    mdns_priv_pcb_announce_CMockIgnore();
    mdns_priv_pcb_send_bye_service_CMockIgnore();
    mdns_priv_pcb_check_probing_services_CMockIgnore();
    mdns_priv_pcb_is_after_probing_IgnoreAndReturn(true);
    mdsn_priv_pcb_is_inited_IgnoreAndReturn(false);

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

    RUN_TEST(test_mdns_discovery_questions_deduped);

    RUN_TEST(test_mdns_aaaa_zone);

    RUN_TEST(test_mdns_aaaa_zone_in_query_results);

    UNITY_END();
}
