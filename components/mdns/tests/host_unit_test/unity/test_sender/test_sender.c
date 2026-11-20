/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>

#include "unity.h"
#include "unity_main.h"
#include "mock_mdns_pcb.h"
#include "mdns.h"
#include "mdns_private.h"
#include "mdns_send.h"
#include "mdns_mem_caps_test.h"
#include "mdns_networking_test.h"

void setup_cmock(void)
{
    mdns_priv_probe_all_pcbs_CMockIgnore();
    mdns_priv_pcb_announce_CMockIgnore();
    mdns_priv_pcb_send_bye_service_CMockIgnore();
    mdns_priv_pcb_check_probing_services_CMockIgnore();
    mdns_priv_pcb_is_after_probing_IgnoreAndReturn(true);
}

static bool packet_contains_dns_label(const uint8_t *packet, size_t len, const char *label)
{
    const size_t label_len = strlen(label);
    if (label_len == 0 || label_len > 63 || len < (label_len + 1)) {
        return false;
    }

    for (size_t i = 0; i + label_len < len; i++) {
        if (packet[i] == (uint8_t)label_len && memcmp(&packet[i + 1], label, label_len) == 0) {
            return true;
        }
    }
    return false;
}

static void test_dispatch_tx_packet(void)
{
    mdns_tx_packet_t p = {};
    mdns_priv_dispatch_tx_packet(&p);
}

static void test_cleanup_tx_action_of_scheduled_packet(void)
{
    size_t baseline = mdns_mem_get_allocation_counts();
    mdns_tx_packet_t *packet = mdns_priv_alloc_packet(0, MDNS_IP_PROTOCOL_V4);
    TEST_ASSERT_NOT_NULL(packet);
    mdns_priv_send_after(packet, 0);

    // State left by mdns_priv_send_packets() for a due packet whose action has not run yet:
    // the packet is marked as queued and stays linked in the TX queue
    packet->queued = true;
    mdns_action_t action = { .type = ACTION_TX_HANDLE };
    action.data.tx_handle.packet = packet;

    // mdns_free() cleans up the pending actions first, then clears the TX queue
    mdns_priv_send_action(&action, ACTION_CLEANUP);
    mdns_priv_clear_tx_queue();

    TEST_ASSERT_EQUAL_size_t(baseline, mdns_mem_get_allocation_counts());
}

/*
 * Subtype goodbye with instance_name == NULL (find-first-service) must use the
 * resolved default instance in the PTR FQDN, same as other send paths.
 */
static void test_subtype_bye_resolves_null_instance(void)
{
    const char *service = "_byetest";
    const char *proto = "_tcp";
    const char *subtype = "gone";
    const char *default_instance = "test2";

    mdns_service_remove_all();
    TEST_ASSERT_EQUAL(ESP_OK, mdns_service_add(NULL, service, proto, 80, NULL, 0));
    TEST_ASSERT_EQUAL(ESP_OK, mdns_service_subtype_add_for_host(NULL, service, proto, NULL, subtype));

    mdns_test_tx_capture_clear();
    TEST_ASSERT_EQUAL(ESP_OK, mdns_service_subtype_remove_for_host(NULL, service, proto, NULL, subtype));

    TEST_ASSERT_GREATER_THAN_size_t(0, mdns_test_tx_capture_count());
    TEST_ASSERT_GREATER_THAN_size_t(MDNS_HEAD_LEN, mdns_test_tx_capture_len());

    const uint8_t *pkt = mdns_test_tx_capture_data();
    const size_t pkt_len = mdns_test_tx_capture_len();
    const uint16_t answers = ((uint16_t)pkt[MDNS_HEAD_ANSWERS_OFFSET] << 8) | pkt[MDNS_HEAD_ANSWERS_OFFSET + 1];

    TEST_ASSERT_EQUAL_UINT16(1, answers);
    TEST_ASSERT_TRUE(packet_contains_dns_label(pkt, pkt_len, subtype));
    TEST_ASSERT_TRUE(packet_contains_dns_label(pkt, pkt_len, "_sub"));
    TEST_ASSERT_TRUE(packet_contains_dns_label(pkt, pkt_len, default_instance));
    TEST_ASSERT_TRUE(packet_contains_dns_label(pkt, pkt_len, service));
    TEST_ASSERT_TRUE(packet_contains_dns_label(pkt, pkt_len, proto));

    mdns_service_remove_all();
}

void run_unity_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_dispatch_tx_packet);
    RUN_TEST(test_cleanup_tx_action_of_scheduled_packet);
    RUN_TEST(test_subtype_bye_resolves_null_instance);

    UNITY_END();
}
