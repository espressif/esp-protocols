/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "net_connect.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "unity.h"
#include "test_utils.h"
#include "unity_fixture.h"
#include "memory_checks.h"

#define TEST_NETIF_DESC_STA "net_connect_netif_sta"
#define TEST_NETIF_DESC_ETH "net_connect_netif_eth"
#define TEST_NETIF_DESC_THREAD "net_connect_netif_thread"
#define TEST_NETIF_DESC_PPP "net_connect_netif_ppp"
#define TEST_NETIF_DESC_OTHER "other_netif_desc"

TEST_GROUP(net_connect);

static esp_netif_t *test_netif_sta = NULL;
static esp_netif_t *test_netif_eth = NULL;
static esp_netif_t *test_netif_thread = NULL;
static esp_netif_t *test_netif_ppp = NULL;
static esp_netif_t *test_netif_other = NULL;

TEST_SETUP(net_connect)
{
    test_utils_record_free_mem();
    TEST_ESP_OK(test_utils_set_leak_level(0, ESP_LEAK_TYPE_CRITICAL, ESP_COMP_LEAK_GENERAL));

    // Create default event loop for netif operations
    esp_event_loop_create_default();

    // Create test netifs with different descriptions
    // Using WiFi STA config for all as it's the simplest and most reliable for unit tests
    // Set unique if_key and description in the inherent config before creating the netif
    // Note: if_key must be unique for each interface, while if_desc is what we search by
    esp_netif_inherent_config_t base_cfg_sta = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base_cfg_sta.if_key = "WIFI_STA_TEST_STA";
    base_cfg_sta.if_desc = TEST_NETIF_DESC_STA;
    esp_netif_config_t cfg_sta = {
        .base = &base_cfg_sta,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    test_netif_sta = esp_netif_new(&cfg_sta);
    TEST_ASSERT_NOT_NULL(test_netif_sta);

    esp_netif_inherent_config_t base_cfg_eth = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base_cfg_eth.if_key = "WIFI_STA_TEST_ETH";
    base_cfg_eth.if_desc = TEST_NETIF_DESC_ETH;
    esp_netif_config_t cfg_eth = {
        .base = &base_cfg_eth,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    test_netif_eth = esp_netif_new(&cfg_eth);
    TEST_ASSERT_NOT_NULL(test_netif_eth);

    esp_netif_inherent_config_t base_cfg_thread = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base_cfg_thread.if_key = "WIFI_STA_TEST_THREAD";
    base_cfg_thread.if_desc = TEST_NETIF_DESC_THREAD;
    esp_netif_config_t cfg_thread = {
        .base = &base_cfg_thread,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    test_netif_thread = esp_netif_new(&cfg_thread);
    TEST_ASSERT_NOT_NULL(test_netif_thread);

    esp_netif_inherent_config_t base_cfg_ppp = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base_cfg_ppp.if_key = "WIFI_STA_TEST_PPP";
    base_cfg_ppp.if_desc = TEST_NETIF_DESC_PPP;
    esp_netif_config_t cfg_ppp = {
        .base = &base_cfg_ppp,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    test_netif_ppp = esp_netif_new(&cfg_ppp);
    TEST_ASSERT_NOT_NULL(test_netif_ppp);

    esp_netif_inherent_config_t base_cfg_other = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base_cfg_other.if_key = "WIFI_STA_TEST_OTHER";
    base_cfg_other.if_desc = TEST_NETIF_DESC_OTHER;
    esp_netif_config_t cfg_other = {
        .base = &base_cfg_other,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    test_netif_other = esp_netif_new(&cfg_other);
    TEST_ASSERT_NOT_NULL(test_netif_other);
}

TEST_TEAR_DOWN(net_connect)
{
    // Clean up test netifs
    if (test_netif_sta) {
        esp_netif_destroy(test_netif_sta);
        test_netif_sta = NULL;
    }
    if (test_netif_eth) {
        esp_netif_destroy(test_netif_eth);
        test_netif_eth = NULL;
    }
    if (test_netif_thread) {
        esp_netif_destroy(test_netif_thread);
        test_netif_thread = NULL;
    }
    if (test_netif_ppp) {
        esp_netif_destroy(test_netif_ppp);
        test_netif_ppp = NULL;
    }
    if (test_netif_other) {
        esp_netif_destroy(test_netif_other);
        test_netif_other = NULL;
    }

    esp_event_loop_delete_default();
    test_utils_finish_and_evaluate_leaks(0, 0);
}

/**
 * @brief Test that net_get_netif_from_desc() can successfully find a WiFi STA netif by its description
 *
 * Objective: Verify that the function correctly locates a network interface with description
 * "net_connect_netif_sta" and returns the correct netif pointer. This ensures basic lookup
 * functionality works for STA interfaces.
 */
TEST(net_connect, net_get_netif_from_desc_finds_sta)
{
    esp_netif_t *found = net_get_netif_from_desc(TEST_NETIF_DESC_STA);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL(test_netif_sta, found);
    TEST_ASSERT_EQUAL_STRING(TEST_NETIF_DESC_STA, esp_netif_get_desc(found));
}

/**
 * @brief Test that net_get_netif_from_desc() can successfully find an Ethernet netif by its description
 *
 * Objective: Verify that the function correctly locates a network interface with description
 * "net_connect_netif_eth" and returns the correct netif pointer. This ensures lookup functionality
 * works for Ethernet interfaces.
 */
TEST(net_connect, net_get_netif_from_desc_finds_eth)
{
    esp_netif_t *found = net_get_netif_from_desc(TEST_NETIF_DESC_ETH);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL(test_netif_eth, found);
    TEST_ASSERT_EQUAL_STRING(TEST_NETIF_DESC_ETH, esp_netif_get_desc(found));
}

/**
 * @brief Test that net_get_netif_from_desc() can successfully find a Thread netif by its description
 *
 * Objective: Verify that the function correctly locates a network interface with description
 * "net_connect_netif_thread" and returns the correct netif pointer. This ensures lookup functionality
 * works for Thread interfaces.
 */
TEST(net_connect, net_get_netif_from_desc_finds_thread)
{
    esp_netif_t *found = net_get_netif_from_desc(TEST_NETIF_DESC_THREAD);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL(test_netif_thread, found);
    TEST_ASSERT_EQUAL_STRING(TEST_NETIF_DESC_THREAD, esp_netif_get_desc(found));
}

/**
 * @brief Test that net_get_netif_from_desc() can successfully find a PPP netif by its description
 *
 * Objective: Verify that the function correctly locates a network interface with description
 * "net_connect_netif_ppp" and returns the correct netif pointer. This ensures lookup functionality
 * works for PPP interfaces.
 */
TEST(net_connect, net_get_netif_from_desc_finds_ppp)
{
    esp_netif_t *found = net_get_netif_from_desc(TEST_NETIF_DESC_PPP);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL(test_netif_ppp, found);
    TEST_ASSERT_EQUAL_STRING(TEST_NETIF_DESC_PPP, esp_netif_get_desc(found));
}

/**
 * @brief Test that net_get_netif_from_desc() can successfully find a netif with a custom description
 *
 * Objective: Verify that the function correctly locates a network interface with a custom description
 * "other_netif_desc" and returns the correct netif pointer. This ensures lookup functionality works
 * for interfaces with non-standard descriptions.
 */
TEST(net_connect, net_get_netif_from_desc_finds_other)
{
    esp_netif_t *found = net_get_netif_from_desc(TEST_NETIF_DESC_OTHER);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL(test_netif_other, found);
    TEST_ASSERT_EQUAL_STRING(TEST_NETIF_DESC_OTHER, esp_netif_get_desc(found));
}

/**
 * @brief Test that net_get_netif_from_desc() returns NULL when searching for a non-existent netif description
 *
 * Objective: Verify that the function correctly handles the case where no network interface matches
 * the provided description. This ensures proper error handling when a netif doesn't exist.
 */
TEST(net_connect, net_get_netif_from_desc_returns_null_for_nonexistent)
{
    esp_netif_t *found = net_get_netif_from_desc("nonexistent_netif_desc");
    TEST_ASSERT_NULL(found);
}

/**
 * @brief Test that net_get_netif_from_desc() returns NULL when passed a NULL description pointer
 *
 * Objective: Verify that the function safely handles NULL input and returns NULL instead of
 * crashing or dereferencing a NULL pointer. This ensures robust error handling for invalid inputs.
 */
TEST(net_connect, net_get_netif_from_desc_returns_null_for_null_desc)
{
    esp_netif_t *found = net_get_netif_from_desc(NULL);
    TEST_ASSERT_NULL(found);
}

/**
 * @brief Test that net_get_netif_from_desc() returns NULL when passed an empty string description
 *
 * Objective: Verify that the function correctly handles empty string input and returns NULL.
 * This ensures that empty descriptions are treated as invalid and don't match any netif.
 */
TEST(net_connect, net_get_netif_from_desc_returns_null_for_empty_desc)
{
    esp_netif_t *found = net_get_netif_from_desc("");
    TEST_ASSERT_NULL(found);
}

/**
 * @brief Test that net_get_netif_from_desc() performs case-sensitive matching
 *
 * Objective: Verify that description matching is case-sensitive. An uppercase version of a valid
 * description should not match, while the exact case-sensitive description should match. This ensures
 * that the function requires exact string matches including case.
 */
TEST(net_connect, net_get_netif_from_desc_case_sensitive)
{
    // Test that description matching is case-sensitive
    esp_netif_t *found_upper = net_get_netif_from_desc("NET_CONNECT_NETIF_STA");
    TEST_ASSERT_NULL(found_upper);

    esp_netif_t *found_lower = net_get_netif_from_desc(TEST_NETIF_DESC_STA);
    TEST_ASSERT_NOT_NULL(found_lower);
}

/**
 * @brief Test that net_get_netif_from_desc() requires exact string matches (no partial matching)
 *
 * Objective: Verify that the function only matches exact descriptions and rejects partial matches.
 * Both a prefix substring and a string with extra characters should fail to match. This ensures
 * that the lookup requires precise description strings.
 */
TEST(net_connect, net_get_netif_from_desc_partial_match_fails)
{
    // Test that partial matches don't work (must be exact match)
    esp_netif_t *found = net_get_netif_from_desc("net_connect_netif_st");
    TEST_ASSERT_NULL(found);

    found = net_get_netif_from_desc("net_connect_netif_sta_extra");
    TEST_ASSERT_NULL(found);
}

/**
 * @brief Test that net_get_netif_from_desc() returns consistent results across multiple calls
 *
 * Objective: Verify that calling the function multiple times with the same description returns
 * the same netif pointer each time. This ensures the function's behavior is deterministic and
 * consistent, which is important for reliability in production code.
 */
TEST(net_connect, net_get_netif_from_desc_multiple_calls_same_result)
{
    // Test that multiple calls return the same result
    esp_netif_t *found1 = net_get_netif_from_desc(TEST_NETIF_DESC_STA);
    esp_netif_t *found2 = net_get_netif_from_desc(TEST_NETIF_DESC_STA);
    esp_netif_t *found3 = net_get_netif_from_desc(TEST_NETIF_DESC_STA);

    TEST_ASSERT_NOT_NULL(found1);
    TEST_ASSERT_EQUAL(found1, found2);
    TEST_ASSERT_EQUAL(found2, found3);
    TEST_ASSERT_EQUAL(test_netif_sta, found1);
}

TEST_GROUP_RUNNER(net_connect)
{
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_finds_sta);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_finds_eth);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_finds_thread);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_finds_ppp);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_finds_other);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_returns_null_for_nonexistent);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_returns_null_for_null_desc);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_returns_null_for_empty_desc);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_case_sensitive);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_partial_match_fails);
    RUN_TEST_CASE(net_connect, net_get_netif_from_desc_multiple_calls_same_result);
}

void app_main(void)
{
    UNITY_MAIN(net_connect);
}
