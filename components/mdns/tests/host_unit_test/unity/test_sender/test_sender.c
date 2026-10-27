/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "unity.h"
#include "unity_main.h"
#include "mock_mdns_pcb.h"
#include "mdns_send.h"
#include "mdns_mem_caps_test.h"

void setup_cmock(void)
{
    mdns_priv_probe_all_pcbs_CMockIgnore();
    mdns_priv_pcb_announce_CMockIgnore();
    mdns_priv_pcb_send_bye_service_CMockIgnore();
    mdns_priv_pcb_check_probing_services_CMockIgnore();
    mdns_priv_pcb_is_after_probing_IgnoreAndReturn(true);
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

void run_unity_tests(void)
{
    UNITY_BEGIN();

    // Run hostname queries test
    RUN_TEST(test_dispatch_tx_packet);
    RUN_TEST(test_cleanup_tx_action_of_scheduled_packet);


    UNITY_END();
}
