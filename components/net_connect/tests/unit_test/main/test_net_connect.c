/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "net_connect.h"
#include "net_iface_priv.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
/* Route-priority test netifs. Destroyed by that test, or by tear-down if it fails. */
static esp_netif_t *s_prio_eth_netif = NULL;
static esp_netif_t *s_prio_wifi_netif = NULL;

#define TEST_EVENT_LOG_LEN 32

static net_iface_handle_t s_test_handle = NULL;
static net_iface_handle_t s_test_handle2 = NULL;
static net_iface_handle_t s_test_handle3 = NULL;
static net_iface_handle_t s_test_handle4 = NULL;
static bool s_handler_registered = false;
static volatile int s_event_count = 0;
static net_iface_event_id_t s_event_ids[TEST_EVENT_LOG_LEN];
static net_iface_handle_t s_event_ifaces[TEST_EVENT_LOG_LEN];

static void test_on_iface_event(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    if (event_data == NULL) {
        return;
    }
    int idx = s_event_count;
    if (idx >= TEST_EVENT_LOG_LEN) {
        return;
    }
    const net_iface_event_data_t *data = event_data;
    s_event_ids[idx] = (net_iface_event_id_t)event_id;
    s_event_ifaces[idx] = data->iface;
    s_event_count = idx + 1;
}

static void test_reset_events(void)
{
    s_event_count = 0;
}

static void test_unregister_handler(void)
{
    if (!s_handler_registered) {
        return;
    }
    net_iface_unregister_event_handler(test_on_iface_event);
    s_handler_registered = false;
}

static esp_err_t test_register_handler(void)
{
    test_reset_events();
    esp_err_t err = net_iface_register_event_handler(test_on_iface_event, NULL);
    if (err == ESP_OK) {
        s_handler_registered = true;
    }
    return err;
}

static void test_clear_deadlock_handler(void);
static void test_clear_prio_handler(void);

static void test_iface_cleanup(void)
{
    test_clear_deadlock_handler();
    test_clear_prio_handler();
    net_iface_handle_t handles[4] = {
        s_test_handle, s_test_handle2, s_test_handle3, s_test_handle4
    };
    s_test_handle = NULL;
    s_test_handle2 = NULL;
    s_test_handle3 = NULL;
    s_test_handle4 = NULL;
    for (int i = 0; i < 4; i++) {
        net_iface_handle_t h = handles[i];
        if (h == NULL) {
            continue;
        }
        net_iface_state_t state = net_iface_get_state(h);
        if (state != NET_IFACE_STATE_INIT && state != NET_IFACE_STATE_UNINIT) {
            net_iface_stop(h);
        }
        net_iface_deinit(h);
    }
    test_unregister_handler();
}

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
    /* Drop iface handles before the default event loop so global IP handlers unregister. */
    test_iface_cleanup();

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
    if (s_prio_wifi_netif != NULL) {
        esp_netif_destroy(s_prio_wifi_netif);
        s_prio_wifi_netif = NULL;
    }
    if (s_prio_eth_netif != NULL) {
        esp_netif_destroy(s_prio_eth_netif);
        s_prio_eth_netif = NULL;
    }

    esp_event_loop_delete_default();
    /*
     * A startup task deletes itself. The idle task frees its stack. Yield so
     * that happens before the leak check.
     */
    vTaskDelay(pdMS_TO_TICKS(20));
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

static esp_err_t fake_start(net_iface_handle_t self)
{
    /* Use test_netif_sta created in TEST_SETUP as the fake netif */
    net_iface_notify_started(self, test_netif_sta);
    return ESP_OK;
}

static esp_err_t fake_stop(net_iface_handle_t self)
{
    (void)self;
    return ESP_OK;
}

static esp_err_t fake_connect(net_iface_handle_t self)
{
    net_iface_notify_connected(self);
    return ESP_OK;
}

static esp_err_t fake_disconnect(net_iface_handle_t self)
{
    (void)self;
    return ESP_OK;
}

static const net_iface_ops_t s_fake_ops = {
    .name = "fake_wifi",
    .init = NULL,
    .deinit = NULL,
    .start = fake_start,
    .stop = fake_stop,
    .connect = fake_connect,
    .disconnect = fake_disconnect,
};

static esp_err_t open_fake_iface(net_iface_handle_t *out, bool enable_ipv6)
{
    net_iface_config_t cfg = { 0 };
    cfg.type = NET_IFACE_TYPE_WIFI_STA;
    cfg.desc = TEST_NETIF_DESC_STA;
    cfg.ip.use_dhcp = true;
    cfg.ip.enable_ipv6 = enable_ipv6;
    esp_err_t err = net_iface_init_with_ops(&cfg, &s_fake_ops, out);
    if (err == ESP_OK) {
        s_test_handle = *out;
    }
    return err;
}

/* Wait until the background startup task has finished ops->start(). */
static void wait_for_startup_done(net_iface_handle_t h)
{
    xEventGroupWaitBits(h->state_events, NET_IFACE_BIT_STARTUP_DONE,
                        pdFALSE, pdFALSE, pdMS_TO_TICKS(100));
}

static void expect_iface_events(net_iface_handle_t h, const net_iface_event_id_t *expected, int count)
{
    int seen = s_event_count;
    TEST_ASSERT_EQUAL(count, seen);
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL(expected[i], s_event_ids[i]);
        TEST_ASSERT_EQUAL(h, s_event_ifaces[i]);
    }
}

/**
 * @brief Walk INIT -> STARTED -> CONNECTED -> READY -> INIT using a fake driver.
 */
TEST(net_connect, net_iface_lifecycle_state_transitions)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(NET_IFACE_TYPE_WIFI_STA, net_iface_get_type(h));
    TEST_ASSERT_EQUAL(h, net_iface_get_by_type(NET_IFACE_TYPE_WIFI_STA));
    TEST_ASSERT_EQUAL(h, net_iface_get_by_desc(TEST_NETIF_DESC_STA));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, net_iface_connect(h));

    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_STARTED, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(test_netif_sta, net_iface_get_netif(h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, net_iface_deinit(h));

    TEST_ESP_OK(net_iface_connect(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTED, net_iface_get_state(h));

    net_iface_notify_ready(h);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h));

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ASSERT_NULL(net_iface_get_netif(h));

    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief Callbacks observe STARTED, CONNECTED, READY, STOPPED in that order.
 */
TEST(net_connect, net_iface_event_callbacks_and_order)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(test_register_handler());
    TEST_ESP_OK(open_fake_iface(&h, false));

    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));
    net_iface_notify_ready(h);
    TEST_ESP_OK(net_iface_stop(h));
    vTaskDelay(pdMS_TO_TICKS(20));

    const net_iface_event_id_t expected[] = {
        NET_IFACE_EVENT_STARTED,
        NET_IFACE_EVENT_CONNECTED,
        NET_IFACE_EVENT_READY,
        NET_IFACE_EVENT_STOPPED,
    };
    expect_iface_events(h, expected, 4);

    test_unregister_handler();
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief Failure is sticky for net_iface_wait() and is reported as last_err.
 */
TEST(net_connect, net_iface_failure_records_last_error)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));
    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);

    net_iface_notify_failed(h, ESP_ERR_WIFI_CONN);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_FAILED, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, net_iface_get_last_error(h));
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, net_iface_wait(h, 100));

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief A handler registered after READY does not receive a replayed event.
 */
TEST(net_connect, net_iface_late_callback_discovers_state)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));
    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));
    net_iface_notify_ready(h);
    vTaskDelay(pdMS_TO_TICKS(20));

    TEST_ESP_OK(test_register_handler());
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL(0, s_event_count);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h));
    TEST_ESP_OK(net_iface_wait(h, 0));

    test_unregister_handler();
    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief A simulated STA got-IP event marks the iface READY; lost-IP drops it.
 */
TEST(net_connect, net_iface_ip_event_drives_ready_and_lost_ip)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));
    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTED, net_iface_get_state(h));

    ip_event_got_ip_t event = {
        .esp_netif = test_netif_sta,
    };
    TEST_ESP_OK(esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &event, sizeof(event), pdMS_TO_TICKS(100)));
    TEST_ESP_OK(net_iface_wait(h, 500));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h));

    TEST_ESP_OK(esp_event_post(IP_EVENT, IP_EVENT_STA_LOST_IP, &event, sizeof(event), pdMS_TO_TICKS(100)));
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTING, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, net_iface_wait(h, 10));

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

static esp_err_t slow_eth_start(net_iface_handle_t self)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    net_iface_notify_started(self, test_netif_eth);
    net_iface_notify_connected(self);
    net_iface_notify_ready(self);
    return ESP_OK;
}

static esp_err_t slow_eth_parallel_start(net_iface_handle_t self)
{
    vTaskDelay(pdMS_TO_TICKS(600));
    net_iface_notify_started(self, test_netif_eth);
    net_iface_notify_connected(self);
    net_iface_notify_ready(self);
    return ESP_OK;
}

static esp_err_t fast_wifi_start(net_iface_handle_t self)
{
    vTaskDelay(pdMS_TO_TICKS(20));
    net_iface_notify_started(self, test_netif_sta);
    net_iface_notify_connected(self);
    net_iface_notify_ready(self);
    return ESP_OK;
}

static esp_err_t failing_start(net_iface_handle_t self)
{
    (void)self;
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_ERR_NOT_FOUND;
}

static const net_iface_ops_t s_slow_eth_ops = {
    .name = "slow_eth",
    .start = slow_eth_start,
    .stop = fake_stop,
    .disconnect = fake_disconnect,
};

static const net_iface_ops_t s_slow_eth_parallel_ops = {
    .name = "slow_eth",
    .start = slow_eth_parallel_start,
    .stop = fake_stop,
    .disconnect = fake_disconnect,
};

static const net_iface_ops_t s_fast_wifi_ops = {
    .name = "fast_wifi",
    .start = fast_wifi_start,
    .stop = fake_stop,
    .disconnect = fake_disconnect,
};

static const net_iface_ops_t s_failing_start_ops = {
    .name = "fail_start",
    .start = failing_start,
    .stop = fake_stop,
    .disconnect = fake_disconnect,
};

static esp_err_t open_ops_iface(net_iface_handle_t *out, net_iface_type_t type,
                                const char *desc, const net_iface_ops_t *ops,
                                int stack_size)
{
    net_iface_config_t cfg = { 0 };
    cfg.type = type;
    cfg.desc = desc;
    cfg.ip.use_dhcp = true;
    cfg.startup_task_stack_size = stack_size;
    return net_iface_init_with_ops(&cfg, ops, out);
}

/**
 * @brief net_iface_start() returns while a slow driver is still running.
 */
TEST(net_connect, net_iface_start_returns_promptly_while_driver_blocks)
{
    net_iface_handle_t eth = NULL;
    TEST_ESP_OK(open_ops_iface(&eth, NET_IFACE_TYPE_ETH, TEST_NETIF_DESC_ETH, &s_slow_eth_ops, 0));
    s_test_handle = eth;

    TickType_t t0 = xTaskGetTickCount();
    TEST_ESP_OK(net_iface_start(eth));
    uint32_t elapsed_ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    TEST_ASSERT_LESS_THAN_UINT32(50, elapsed_ms);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_STARTING, net_iface_get_state(eth));

    TEST_ESP_OK(net_iface_wait(eth, 1000));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(eth));

    TEST_ESP_OK(net_iface_stop(eth));
    TEST_ESP_OK(net_iface_deinit(eth));
    s_test_handle = NULL;
}

/**
 * @brief A fast interface reaches READY while a slow one started first is still STARTING.
 */
TEST(net_connect, net_iface_parallel_startup_no_head_of_line_blocking)
{
    net_iface_handle_t eth = NULL;
    net_iface_handle_t wifi = NULL;
    TEST_ESP_OK(open_ops_iface(&eth, NET_IFACE_TYPE_ETH, TEST_NETIF_DESC_ETH,
                               &s_slow_eth_parallel_ops, 0));
    s_test_handle = eth;
    TEST_ESP_OK(open_ops_iface(&wifi, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA,
                               &s_fast_wifi_ops, 0));
    s_test_handle2 = wifi;

    TEST_ESP_OK(net_iface_start(eth));
    TEST_ESP_OK(net_iface_start(wifi));

    TEST_ESP_OK(net_iface_wait(wifi, 200));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(wifi));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_STARTING, net_iface_get_state(eth));

    TEST_ESP_OK(net_iface_wait(eth, 1000));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(eth));

    TEST_ESP_OK(net_iface_stop(wifi));
    TEST_ESP_OK(net_iface_stop(eth));
    TEST_ESP_OK(net_iface_deinit(wifi));
    TEST_ESP_OK(net_iface_deinit(eth));
    s_test_handle = NULL;
    s_test_handle2 = NULL;
}

/**
 * @brief A driver failure inside the startup task wakes wait() and posts FAILED.
 */
TEST(net_connect, net_iface_async_startup_failure_notifies_waiter_and_event)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(test_register_handler());
    TEST_ESP_OK(open_ops_iface(&h, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA,
                               &s_failing_start_ops, 0));
    s_test_handle = h;

    TEST_ESP_OK(net_iface_start(h));
    esp_err_t wait_err = net_iface_wait(h, 500);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, wait_err);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_FAILED, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, net_iface_get_last_error(h));

    vTaskDelay(pdMS_TO_TICKS(20));
    bool saw_failed = false;
    for (int i = 0; i < s_event_count; i++) {
        if (s_event_ids[i] == NET_IFACE_EVENT_FAILED && s_event_ifaces[i] == h) {
            saw_failed = true;
        }
    }
    TEST_ASSERT_TRUE(saw_failed);

    test_unregister_handler();
    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief A startup task that cannot be created fails start() with ESP_ERR_NO_MEM.
 */
TEST(net_connect, net_iface_start_task_alloc_failure_returns_no_mem)
{
    net_iface_handle_t h = NULL;
    /* Larger than any ESP32 heap, so xTaskCreate() fails before the task runs. */
    TEST_ESP_OK(open_ops_iface(&h, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA,
                               &s_fake_ops, 16 * 1024 * 1024));
    s_test_handle = h;

    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, net_iface_start(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_FAILED, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, net_iface_get_last_error(h));

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief net_wifi_sta_create(NULL) stores the Kconfig defaults and stays idle.
 *
 * A custom parameter struct overrides the name, retry count, and route priority.
 * Creating a handle does not start the radio.
 */
TEST(net_connect, net_wifi_sta_create_defaults_and_custom_params)
{
    net_iface_handle_t h = NULL;

#if CONFIG_NET_CONNECT_WIFI
    TEST_ESP_OK(net_wifi_sta_create(NULL, &h));
    s_test_handle = h;
    TEST_ASSERT_NOT_NULL(h);
    TEST_ASSERT_EQUAL(NET_IFACE_TYPE_WIFI_STA, net_iface_get_type(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(h, net_iface_get_by_desc(NET_CONNECT_NETIF_DESC_STA));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;

    net_wifi_sta_params_t params = {
        .ssid = "CustomAP",
        .password = "custom-pass",
        .max_retry = 3,
        .route_prio = 10,
        .required = false,
        .auto_connect_on_start = false,
    };
    TEST_ESP_OK(net_wifi_sta_create(&params, &h));
    s_test_handle = h;
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ASSERT_EQUAL_STRING("CustomAP", h->cfg.wifi_sta.ssid);
    TEST_ASSERT_EQUAL_STRING("custom-pass", h->cfg.wifi_sta.password);
    TEST_ASSERT_EQUAL(3, h->cfg.wifi_sta.max_retry);
    TEST_ASSERT_EQUAL(10, h->cfg.route_prio);
    TEST_ASSERT_FALSE(h->cfg.auto_connect_on_start);
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, net_wifi_sta_create(NULL, &h));
    TEST_ASSERT_NULL(h);
#endif
}

/**
 * @brief An empty Wi-Fi name is rejected. A real name is stored.
 *
 * The handle is created with no name, which is the "not configured yet" case.
 */
TEST(net_connect, net_wifi_sta_set_credentials_validation)
{
    net_iface_handle_t h = NULL;

#if CONFIG_NET_CONNECT_WIFI
    net_wifi_sta_params_t params = {
        .ssid = "",
        .password = NULL,
        .max_retry = -1,
        .route_prio = -1,
        .required = false,
        .auto_connect_on_start = false,
    };
    TEST_ESP_OK(net_wifi_sta_create(&params, &h));
    s_test_handle = h;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, net_wifi_sta_set_credentials(h, "", "pass"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, net_wifi_sta_set_credentials(h, NULL, "pass"));
    TEST_ESP_OK(net_wifi_sta_set_credentials(h, "HomeHubAP", "secret123"));
    TEST_ASSERT_EQUAL_STRING("HomeHubAP", h->cfg.wifi_sta.ssid);
    TEST_ASSERT_EQUAL_STRING("secret123", h->cfg.wifi_sta.password);
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
#else
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, net_wifi_sta_set_credentials(NULL, "HomeHubAP", "secret123"));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, net_wifi_sta_create(NULL, &h));
#endif
}

/**
 * @brief Create and free Ethernet, Thread, and PPP handles without hardware.
 *
 * An interface that is turned off in Kconfig returns ESP_ERR_NOT_SUPPORTED.
 */
TEST(net_connect, net_eth_thread_ppp_create_and_deinit)
{
    net_iface_handle_t h = NULL;

#if CONFIG_NET_CONNECT_ETHERNET
    TEST_ESP_OK(net_eth_create(NULL, &h));
    s_test_handle = h;
    TEST_ASSERT_EQUAL(NET_IFACE_TYPE_ETH, net_iface_get_type(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, net_eth_create(NULL, &h));
    TEST_ASSERT_NULL(h);
#endif

#if CONFIG_NET_CONNECT_THREAD
    TEST_ESP_OK(net_thread_create(NULL, &h));
    s_test_handle = h;
    TEST_ASSERT_EQUAL(NET_IFACE_TYPE_THREAD, net_iface_get_type(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
#else
    h = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, net_thread_create(NULL, &h));
    TEST_ASSERT_NULL(h);
#endif

#if CONFIG_NET_CONNECT_PPP
    TEST_ESP_OK(net_ppp_create(NULL, &h));
    s_test_handle = h;
    TEST_ASSERT_EQUAL(NET_IFACE_TYPE_PPP, net_iface_get_type(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
#else
    h = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, net_ppp_create(NULL, &h));
    TEST_ASSERT_NULL(h);
#endif
}

/**
 * @brief net_iface_wait*() returns immediately when an interface is already READY or FAILED.
 *
 * The limit is 50 ms, not the 5000 ms timeout. A 10 ms tick can cross one
 * boundary during three immediate calls, so 20 ms is too tight.
 */
TEST(net_connect, net_iface_wait_immediate_on_ready_and_failed)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));
    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));
    net_iface_notify_ready(h);

    TickType_t t0 = xTaskGetTickCount();
    TEST_ESP_OK(net_iface_wait(h, 5000));
    TEST_ESP_OK(net_iface_wait_all(&h, 1, 5000));
    net_iface_handle_t ready = NULL;
    TEST_ESP_OK(net_iface_wait_any(&h, 1, 5000, &ready));
    uint32_t elapsed_ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    TEST_ASSERT_LESS_THAN_UINT32(50, elapsed_ms);
    TEST_ASSERT_EQUAL(h, ready);

    net_iface_notify_failed(h, ESP_ERR_WIFI_CONN);
    t0 = xTaskGetTickCount();
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, net_iface_wait(h, 5000));
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, net_iface_wait_all(&h, 1, 5000));
    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, net_iface_wait_any(&h, 1, 5000, &ready));
    elapsed_ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;
    TEST_ASSERT_LESS_THAN_UINT32(50, elapsed_ms);
    TEST_ASSERT_NULL(ready);

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

typedef struct {
    net_iface_handle_t iface;
    uint32_t delay_ms;
    esp_err_t err_to_report; /* ESP_OK means notify_ready, otherwise notify_failed */
    SemaphoreHandle_t done_sem;
} async_notify_arg_t;

static void async_notify_task(void *arg)
{
    async_notify_arg_t *ctx = (async_notify_arg_t *)arg;
    if (ctx->delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(ctx->delay_ms));
    } else {
        taskYIELD();
    }
    if (ctx->err_to_report == ESP_OK) {
        net_iface_notify_ready(ctx->iface);
    } else {
        net_iface_notify_failed(ctx->iface, ctx->err_to_report);
    }
    if (ctx->done_sem != NULL) {
        xSemaphoreGive(ctx->done_sem);
    }
    vTaskDelete(NULL);
}

/**
 * @brief notify_ready() just before or during wait() is never missed.
 *
 * Delays are 0, 10, and 20 ms so a 10 ms tick still overlaps the wait.
 */
TEST(net_connect, net_iface_wait_boundary_race_no_lost_wakeup)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));
    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);

    for (int iter = 0; iter < 25; iter++) {
        /* Drop the previous result, then start a new connection. */
        net_iface_notify_disconnected(h);
        TEST_ESP_OK(net_iface_connect(h));

        async_notify_arg_t arg = {
            .iface = h,
            .delay_ms = (uint32_t)(iter % 3) * 10,
            .err_to_report = ESP_OK,
            .done_sem = done_sem,
        };
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(async_notify_task, "race_prod", 3072,
                                              &arg, 5, NULL));
        TEST_ESP_OK(net_iface_wait(h, 200));
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(200)));
        /* Let the idle task free the helper task stack before the next create. */
        vTaskDelay(1);
    }

    vSemaphoreDelete(done_sem);
    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

static esp_err_t stay_connecting_connect(net_iface_handle_t self)
{
    (void)self;
    /* Leave the interface in CONNECTING. The test decides when it finishes. */
    return ESP_OK;
}

static const net_iface_ops_t s_stay_connecting_ops = {
    .name = "stay_conn",
    .start = fake_start,
    .stop = fake_stop,
    .connect = stay_connecting_connect,
    .disconnect = fake_disconnect,
};

/**
 * @brief A timeout does not cancel a connection that is still in progress.
 */
TEST(net_connect, net_iface_wait_timeout_leaves_connection_running)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_ops_iface(&h, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA,
                               &s_stay_connecting_ops, 0));
    s_test_handle = h;

    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTING, net_iface_get_state(h));

    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, net_iface_wait(h, 50));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTING, net_iface_get_state(h));

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);
    async_notify_arg_t arg = {
        .iface = h,
        .delay_ms = 30,
        .err_to_report = ESP_OK,
        .done_sem = done_sem,
    };
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(async_notify_task, "late_rdy", 3072, &arg, 5, NULL));

    TEST_ESP_OK(net_iface_wait(h, 200));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(200)));
    vSemaphoreDelete(done_sem);

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief wait_all() returns the first failure quickly, then succeeds once every interface is ready.
 */
TEST(net_connect, net_iface_wait_all_shared_budget_and_fail_fast)
{
    net_iface_handle_t h1 = NULL;
    net_iface_handle_t h2 = NULL;
    TEST_ESP_OK(open_ops_iface(&h1, NET_IFACE_TYPE_ETH, TEST_NETIF_DESC_ETH,
                               &s_stay_connecting_ops, 0));
    s_test_handle = h1;
    TEST_ESP_OK(open_ops_iface(&h2, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA,
                               &s_stay_connecting_ops, 0));
    s_test_handle2 = h2;

    TEST_ESP_OK(net_iface_start(h1));
    TEST_ESP_OK(net_iface_start(h2));
    wait_for_startup_done(h1);
    wait_for_startup_done(h2);
    TEST_ESP_OK(net_iface_connect(h1));
    TEST_ESP_OK(net_iface_connect(h2));

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);
    async_notify_arg_t fail_arg = {
        .iface = h2,
        .delay_ms = 50,
        .err_to_report = ESP_ERR_WIFI_CONN,
        .done_sem = done_sem,
    };
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(async_notify_task, "fail_h2", 3072, &fail_arg, 5, NULL));

    net_iface_handle_t both[2] = { h1, h2 };
    TickType_t t0 = xTaskGetTickCount();
    esp_err_t err = net_iface_wait_all(both, 2, 1000);
    uint32_t elapsed_ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;

    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, err);
    TEST_ASSERT_LESS_THAN_UINT32(250, elapsed_ms);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(200)));

    /* Reconnect the failed interface and let both become ready. */
    TEST_ESP_OK(net_iface_connect(h2));
    net_iface_notify_ready(h1);
    async_notify_arg_t ready_arg = {
        .iface = h2,
        .delay_ms = 40,
        .err_to_report = ESP_OK,
        .done_sem = done_sem,
    };
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(async_notify_task, "rdy_h2", 3072, &ready_arg, 5, NULL));
    TEST_ESP_OK(net_iface_wait_all(both, 2, 500));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(200)));
    vSemaphoreDelete(done_sem);

    TEST_ESP_OK(net_iface_stop(h2));
    TEST_ESP_OK(net_iface_stop(h1));
    TEST_ESP_OK(net_iface_deinit(h2));
    TEST_ESP_OK(net_iface_deinit(h1));
    s_test_handle = NULL;
    s_test_handle2 = NULL;
}

/**
 * @brief wait_any() returns the first ready interface, and fails once every interface has failed.
 */
TEST(net_connect, net_iface_wait_any_first_ready_and_all_failed)
{
    net_iface_handle_t h1 = NULL;
    net_iface_handle_t h2 = NULL;
    TEST_ESP_OK(open_ops_iface(&h1, NET_IFACE_TYPE_ETH, TEST_NETIF_DESC_ETH,
                               &s_stay_connecting_ops, 0));
    s_test_handle = h1;
    TEST_ESP_OK(open_ops_iface(&h2, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA,
                               &s_stay_connecting_ops, 0));
    s_test_handle2 = h2;

    TEST_ESP_OK(net_iface_start(h1));
    TEST_ESP_OK(net_iface_start(h2));
    wait_for_startup_done(h1);
    wait_for_startup_done(h2);
    TEST_ESP_OK(net_iface_connect(h1));
    TEST_ESP_OK(net_iface_connect(h2));

    /* h1 fails now. h2 becomes ready 40 ms later. wait_any should return h2. */
    net_iface_notify_failed(h1, ESP_ERR_WIFI_CONN);
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(done_sem);
    async_notify_arg_t ready_arg = {
        .iface = h2,
        .delay_ms = 40,
        .err_to_report = ESP_OK,
        .done_sem = done_sem,
    };
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(async_notify_task, "rdy_h2", 3072, &ready_arg, 5, NULL));

    net_iface_handle_t both[2] = { h1, h2 };
    net_iface_handle_t winner = NULL;
    TEST_ESP_OK(net_iface_wait_any(both, 2, 500, &winner));
    TEST_ASSERT_EQUAL(h2, winner);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(200)));

    /* h1 stays failed. h2 fails too. The first failure is returned at once. */
    net_iface_notify_disconnected(h2);
    TEST_ESP_OK(net_iface_connect(h2));
    async_notify_arg_t fail_arg = {
        .iface = h2,
        .delay_ms = 40,
        .err_to_report = ESP_FAIL,
        .done_sem = done_sem,
    };
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(async_notify_task, "fail_h2", 3072, &fail_arg, 5, NULL));

    TickType_t t0 = xTaskGetTickCount();
    esp_err_t err = net_iface_wait_any(both, 2, 1000, &winner);
    uint32_t elapsed_ms = (uint32_t)(xTaskGetTickCount() - t0) * portTICK_PERIOD_MS;

    TEST_ASSERT_EQUAL(ESP_ERR_WIFI_CONN, err);
    TEST_ASSERT_NULL(winner);
    TEST_ASSERT_LESS_THAN_UINT32(250, elapsed_ms);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done_sem, pdMS_TO_TICKS(200)));
    vSemaphoreDelete(done_sem);

    TEST_ESP_OK(net_iface_stop(h2));
    TEST_ESP_OK(net_iface_stop(h1));
    TEST_ESP_OK(net_iface_deinit(h2));
    TEST_ESP_OK(net_iface_deinit(h1));
    s_test_handle = NULL;
    s_test_handle2 = NULL;
}

typedef struct {
    esp_err_t wait_ret;
    esp_err_t wait_all_ret;
    esp_err_t wait_any_ret;
    SemaphoreHandle_t done_sem;
} deadlock_guard_ctx_t;

static deadlock_guard_ctx_t *s_deadlock_ctx = NULL;
static bool s_deadlock_handler_registered = false;

static void on_event_call_wait(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    deadlock_guard_ctx_t *ctx = s_deadlock_ctx;
    if (ctx == NULL || event_id != NET_IFACE_EVENT_CONNECTED || event_data == NULL) {
        return;
    }
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;
    net_iface_handle_t h = data->iface;

    ctx->wait_ret = net_iface_wait(h, 100);
    ctx->wait_all_ret = net_iface_wait_all(&h, 1, 100);
    ctx->wait_any_ret = net_iface_wait_any(&h, 1, 100, NULL);
    xSemaphoreGive(ctx->done_sem);
}

static void test_clear_deadlock_handler(void)
{
    s_deadlock_ctx = NULL;
    if (!s_deadlock_handler_registered) {
        return;
    }
    net_iface_unregister_event_handler(on_event_call_wait);
    s_deadlock_handler_registered = false;
}

/**
 * @brief A wait from inside a NET_IFACE_EVENT handler returns ESP_ERR_INVALID_STATE.
 */
TEST(net_connect, net_iface_wait_from_event_loop_returns_invalid_state)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));

    deadlock_guard_ctx_t ctx = {
        .wait_ret = ESP_OK,
        .wait_all_ret = ESP_OK,
        .wait_any_ret = ESP_OK,
        .done_sem = xSemaphoreCreateBinary(),
    };
    TEST_ASSERT_NOT_NULL(ctx.done_sem);
    s_deadlock_ctx = &ctx;
    TEST_ESP_OK(net_iface_register_event_handler(on_event_call_wait, NULL));
    s_deadlock_handler_registered = true;

    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.done_sem, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ctx.wait_ret);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ctx.wait_all_ret);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ctx.wait_any_ret);

    test_clear_deadlock_handler();
    vSemaphoreDelete(ctx.done_sem);
    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

static int s_cancel_start_calls = 0;
static int s_cancel_connect_calls = 0;
static int s_cancel_stop_calls = 0;

static esp_err_t slow_cancelable_start(net_iface_handle_t self)
{
    s_cancel_start_calls++;
    vTaskDelay(pdMS_TO_TICKS(250));
    net_iface_notify_started(self, test_netif_eth);
    return ESP_OK;
}

static esp_err_t slow_cancelable_connect(net_iface_handle_t self)
{
    s_cancel_connect_calls++;
    net_iface_notify_connected(self);
    return ESP_OK;
}

static esp_err_t slow_cancelable_stop(net_iface_handle_t self)
{
    (void)self;
    s_cancel_stop_calls++;
    return ESP_OK;
}

static const net_iface_ops_t s_slow_cancelable_ops = {
    .name = "slow_cancel",
    .start = slow_cancelable_start,
    .stop = slow_cancelable_stop,
    .connect = slow_cancelable_connect,
    .disconnect = fake_disconnect,
};

/**
 * @brief stop() during STARTING waits for start(), skips connect(), and runs stop().
 */
TEST(net_connect, net_iface_stop_during_starting_cancels_cleanly)
{
    s_cancel_start_calls = 0;
    s_cancel_connect_calls = 0;
    s_cancel_stop_calls = 0;

    net_iface_config_t cfg = { 0 };
    cfg.type = NET_IFACE_TYPE_ETH;
    cfg.desc = TEST_NETIF_DESC_ETH;
    cfg.auto_connect_on_start = true;
    cfg.ip.use_dhcp = true;

    net_iface_handle_t h = NULL;
    TEST_ESP_OK(test_register_handler());
    TEST_ESP_OK(net_iface_init_with_ops(&cfg, &s_slow_cancelable_ops, &h));
    s_test_handle = h;

    TEST_ESP_OK(net_iface_start(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_STARTING, net_iface_get_state(h));

    /* A second start while the first is still running is rejected. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, net_iface_start(h));

    /* Let the startup task enter the slow start, then stop it. */
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ESP_OK(net_iface_stop(h));

    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(1, s_cancel_start_calls);
    TEST_ASSERT_EQUAL(0, s_cancel_connect_calls);
    TEST_ASSERT_EQUAL(1, s_cancel_stop_calls);

    vTaskDelay(pdMS_TO_TICKS(20));
    /* STARTED and CONNECTED stay suppressed. Only STOPPED is posted. */
    const net_iface_event_id_t expected[] = {
        NET_IFACE_EVENT_STOPPED,
    };
    expect_iface_events(h, expected, 1);

    test_unregister_handler();
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief Ten init, start, wait, stop, and deinit cycles on two interfaces leak nothing.
 */
TEST(net_connect, net_iface_repeated_start_stop_deinit_cycles)
{
    for (int i = 0; i < 10; i++) {
        net_iface_handle_t h1 = NULL;
        net_iface_handle_t h2 = NULL;
        TEST_ESP_OK(open_ops_iface(&h1, NET_IFACE_TYPE_ETH, TEST_NETIF_DESC_ETH, &s_fake_ops, 0));
        s_test_handle = h1;
        TEST_ESP_OK(open_ops_iface(&h2, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA, &s_fake_ops, 0));
        s_test_handle2 = h2;

        TEST_ESP_OK(net_iface_start(h1));
        TEST_ESP_OK(net_iface_start(h2));
        wait_for_startup_done(h1);
        wait_for_startup_done(h2);

        TEST_ESP_OK(net_iface_connect(h1));
        TEST_ESP_OK(net_iface_connect(h2));
        net_iface_notify_ready(h1);
        net_iface_notify_ready(h2);

        net_iface_handle_t both[2] = { h1, h2 };
        TEST_ESP_OK(net_iface_wait_all(both, 2, 200));

        TEST_ESP_OK(net_iface_stop(h2));
        TEST_ESP_OK(net_iface_stop(h1));
        TEST_ESP_OK(net_iface_deinit(h2));
        TEST_ESP_OK(net_iface_deinit(h1));
        s_test_handle = NULL;
        s_test_handle2 = NULL;
        /* Let the idle task free the two startup-task stacks before the next pass. */
        vTaskDelay(1);
    }
}

static bool s_flaky_should_fail = true;

static esp_err_t flaky_start(net_iface_handle_t self)
{
    if (s_flaky_should_fail) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    net_iface_notify_started(self, test_netif_sta);
    return ESP_OK;
}

static const net_iface_ops_t s_flaky_ops = {
    .name = "flaky_wifi",
    .start = flaky_start,
    .stop = fake_stop,
    .connect = fake_connect,
    .disconnect = fake_disconnect,
};

/**
 * @brief A handle whose start() failed can be started again from FAILED and reach READY.
 */
TEST(net_connect, net_iface_restart_after_failed_start_succeeds)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_ops_iface(&h, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA, &s_flaky_ops, 0));
    s_test_handle = h;

    /* First attempt fails inside ops->start. */
    s_flaky_should_fail = true;
    TEST_ESP_OK(net_iface_start(h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, net_iface_wait(h, 500));
    /* The startup task clears its handle after reporting the failure. */
    wait_for_startup_done(h);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_FAILED, net_iface_get_state(h));

    /* A later start from FAILED does not need stop() first. */
    s_flaky_should_fail = false;
    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_STARTED, net_iface_get_state(h));
    TEST_ESP_OK(net_iface_connect(h));
    net_iface_notify_ready(h);
    TEST_ESP_OK(net_iface_wait(h, 200));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(ESP_OK, net_iface_get_last_error(h));

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/* ============================================================
 *       Stage 7: regression and integration tests
 * ============================================================ */

static esp_err_t fake_eth_start(net_iface_handle_t self)
{
    net_iface_notify_started(self, test_netif_eth);
    return ESP_OK;
}

static esp_err_t fake_thread_start(net_iface_handle_t self)
{
    net_iface_notify_started(self, test_netif_thread);
    return ESP_OK;
}

static esp_err_t fake_ppp_start(net_iface_handle_t self)
{
    net_iface_notify_started(self, test_netif_ppp);
    return ESP_OK;
}

static const net_iface_ops_t s_fake_eth_ops = {
    .name = "fake_eth",
    .start = fake_eth_start,
    .stop = fake_stop,
    .connect = stay_connecting_connect,
    .disconnect = fake_disconnect,
};

static const net_iface_ops_t s_fake_thread_ops = {
    .name = "fake_thread",
    .start = fake_thread_start,
    .stop = fake_stop,
    .connect = stay_connecting_connect,
    .disconnect = fake_disconnect,
};

static const net_iface_ops_t s_fake_ppp_ops = {
    .name = "fake_ppp",
    .start = fake_ppp_start,
    .stop = fake_stop,
    .connect = stay_connecting_connect,
    .disconnect = fake_disconnect,
};

typedef struct {
    net_iface_handle_t iface;
    uint32_t delay_ms;
    SemaphoreHandle_t done_sem;
} drive_iface_arg_t;

static void drive_iface_to_ready_task(void *arg)
{
    drive_iface_arg_t *ctx = (drive_iface_arg_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(ctx->delay_ms));
    net_iface_notify_connected(ctx->iface);
    vTaskDelay(pdMS_TO_TICKS(5));
    net_iface_notify_ready(ctx->iface);
    xSemaphoreGive(ctx->done_sem);
    vTaskDelete(NULL);
}

typedef struct {
    const net_iface_handle_t *handles;
    size_t count;
    bool wait_all;
    esp_err_t result;
    net_iface_handle_t ready_out;
    SemaphoreHandle_t done_sem;
} multi_waiter_arg_t;

static void multi_waiter_task(void *arg)
{
    multi_waiter_arg_t *ctx = (multi_waiter_arg_t *)arg;
    if (ctx->wait_all) {
        ctx->result = net_iface_wait_all(ctx->handles, ctx->count, 1000);
    } else {
        ctx->result = net_iface_wait_any(ctx->handles, ctx->count, 1000, &ctx->ready_out);
    }
    xSemaphoreGive(ctx->done_sem);
    vTaskDelete(NULL);
}

/**
 * @brief Four interfaces reach READY together while wait_all() and wait_any() are waiting.
 */
TEST(net_connect, net_iface_four_interfaces_concurrent_events_and_waiters)
{
    net_iface_handle_t h_eth = NULL;
    net_iface_handle_t h_wifi = NULL;
    net_iface_handle_t h_thread = NULL;
    net_iface_handle_t h_ppp = NULL;

    TEST_ESP_OK(test_register_handler());
    TEST_ESP_OK(open_ops_iface(&h_eth, NET_IFACE_TYPE_ETH, TEST_NETIF_DESC_ETH, &s_fake_eth_ops, 0));
    s_test_handle = h_eth;
    TEST_ESP_OK(open_ops_iface(&h_wifi, NET_IFACE_TYPE_WIFI_STA, TEST_NETIF_DESC_STA, &s_stay_connecting_ops, 0));
    s_test_handle2 = h_wifi;
    TEST_ESP_OK(open_ops_iface(&h_thread, NET_IFACE_TYPE_THREAD, TEST_NETIF_DESC_THREAD, &s_fake_thread_ops, 0));
    s_test_handle3 = h_thread;
    TEST_ESP_OK(open_ops_iface(&h_ppp, NET_IFACE_TYPE_PPP, TEST_NETIF_DESC_PPP, &s_fake_ppp_ops, 0));
    s_test_handle4 = h_ppp;

    net_iface_handle_t all[4] = { h_eth, h_wifi, h_thread, h_ppp };
    for (int i = 0; i < 4; i++) {
        TEST_ESP_OK(net_iface_start(all[i]));
    }
    for (int i = 0; i < 4; i++) {
        wait_for_startup_done(all[i]);
        TEST_ESP_OK(net_iface_connect(all[i]));
    }
    /* Let STARTED land in the event log before the driver tasks post CONNECTED. */
    vTaskDelay(pdMS_TO_TICKS(20));

    SemaphoreHandle_t waiter_all_sem = xSemaphoreCreateBinary();
    SemaphoreHandle_t waiter_any_sem = xSemaphoreCreateBinary();
    SemaphoreHandle_t drv_sems[4] = { NULL, NULL, NULL, NULL };
    TEST_ASSERT_NOT_NULL(waiter_all_sem);
    TEST_ASSERT_NOT_NULL(waiter_any_sem);

    multi_waiter_arg_t wait_all_arg = {
        .handles = all,
        .count = 4,
        .wait_all = true,
        .result = ESP_FAIL,
        .ready_out = NULL,
        .done_sem = waiter_all_sem,
    };
    multi_waiter_arg_t wait_any_arg = {
        .handles = all,
        .count = 4,
        .wait_all = false,
        .result = ESP_FAIL,
        .ready_out = NULL,
        .done_sem = waiter_any_sem,
    };

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(multi_waiter_task, "w_all", 3072, &wait_all_arg, 5, NULL));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(multi_waiter_task, "w_any", 3072, &wait_any_arg, 5, NULL));

    drive_iface_arg_t drv_args[4];
    for (int i = 0; i < 4; i++) {
        drv_sems[i] = xSemaphoreCreateBinary();
        TEST_ASSERT_NOT_NULL(drv_sems[i]);
        drv_args[i].iface = all[i];
        drv_args[i].delay_ms = (uint32_t)(i + 1) * 10;
        drv_args[i].done_sem = drv_sems[i];
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(drive_iface_to_ready_task, "drv_rdy", 3072, &drv_args[i], 5, NULL));
    }

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(waiter_any_sem, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(waiter_all_sem, pdMS_TO_TICKS(1000)));
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(drv_sems[i], pdMS_TO_TICKS(1000)));
        vSemaphoreDelete(drv_sems[i]);
    }
    vSemaphoreDelete(waiter_any_sem);
    vSemaphoreDelete(waiter_all_sem);

    TEST_ESP_OK(wait_any_arg.result);
    TEST_ASSERT_NOT_NULL(wait_any_arg.ready_out);
    TEST_ESP_OK(wait_all_arg.result);

    vTaskDelay(pdMS_TO_TICKS(30));

    /* Each interface emits STARTED, then CONNECTED, then READY. */
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(all[i]));
        net_iface_event_id_t seen_for_iface[8];
        int seen_count = 0;
        for (int e = 0; e < s_event_count; e++) {
            if (s_event_ifaces[e] == all[i] && seen_count < 8) {
                seen_for_iface[seen_count++] = s_event_ids[e];
            }
        }
        TEST_ASSERT_EQUAL(3, seen_count);
        TEST_ASSERT_EQUAL(NET_IFACE_EVENT_STARTED, seen_for_iface[0]);
        TEST_ASSERT_EQUAL(NET_IFACE_EVENT_CONNECTED, seen_for_iface[1]);
        TEST_ASSERT_EQUAL(NET_IFACE_EVENT_READY, seen_for_iface[2]);
    }

    test_unregister_handler();
    for (int i = 3; i >= 0; i--) {
        TEST_ESP_OK(net_iface_stop(all[i]));
        TEST_ESP_OK(net_iface_deinit(all[i]));
    }
    s_test_handle = NULL;
    s_test_handle2 = NULL;
    s_test_handle3 = NULL;
    s_test_handle4 = NULL;
    /* Let the idle task free the helper-task stacks before the leak check. */
    vTaskDelay(pdMS_TO_TICKS(20));
}

static esp_err_t dummy_netif_transmit(void *h, void *buffer, size_t len)
{
    (void)h;
    (void)buffer;
    (void)len;
    return ESP_OK;
}

static bool s_boost_eth_prio_on_ready = false;
static bool s_prio_handler_registered = false;

static void on_ready_boost_eth_prio(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    if (!s_boost_eth_prio_on_ready || event_id != NET_IFACE_EVENT_READY || event_data == NULL) {
        return;
    }
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;
    if (data->type == NET_IFACE_TYPE_ETH && data->netif != NULL) {
        esp_netif_set_route_prio(data->netif, 200);
        esp_netif_set_default_netif(NULL);
    }
}

static void test_clear_prio_handler(void)
{
    s_boost_eth_prio_on_ready = false;
    if (!s_prio_handler_registered) {
        return;
    }
    net_iface_unregister_event_handler(on_ready_boost_eth_prio);
    s_prio_handler_registered = false;
}

static esp_err_t prio_eth_start(net_iface_handle_t self)
{
    esp_netif_action_start(s_prio_eth_netif, 0, 0, NULL);
    net_iface_notify_started(self, s_prio_eth_netif);
    return ESP_OK;
}

static esp_err_t prio_eth_connect(net_iface_handle_t self)
{
    esp_netif_action_connected(s_prio_eth_netif, 0, 0, NULL);
    net_iface_notify_connected(self);
    net_iface_notify_ready(self);
    return ESP_OK;
}

static esp_err_t prio_eth_stop(net_iface_handle_t self)
{
    (void)self;
    if (s_prio_eth_netif != NULL) {
        esp_netif_action_disconnected(s_prio_eth_netif, 0, 0, NULL);
        esp_netif_action_stop(s_prio_eth_netif, 0, 0, NULL);
    }
    return ESP_OK;
}

static esp_err_t prio_wifi_start(net_iface_handle_t self)
{
    esp_netif_action_start(s_prio_wifi_netif, 0, 0, NULL);
    net_iface_notify_started(self, s_prio_wifi_netif);
    return ESP_OK;
}

static esp_err_t prio_wifi_connect(net_iface_handle_t self)
{
    esp_netif_action_connected(s_prio_wifi_netif, 0, 0, NULL);
    net_iface_notify_connected(self);
    net_iface_notify_ready(self);
    return ESP_OK;
}

static esp_err_t prio_wifi_stop(net_iface_handle_t self)
{
    (void)self;
    if (s_prio_wifi_netif != NULL) {
        esp_netif_action_disconnected(s_prio_wifi_netif, 0, 0, NULL);
        esp_netif_action_stop(s_prio_wifi_netif, 0, 0, NULL);
    }
    return ESP_OK;
}

static const net_iface_ops_t s_prio_eth_ops = {
    .name = "prio_eth",
    .start = prio_eth_start,
    .stop = prio_eth_stop,
    .connect = prio_eth_connect,
    .disconnect = fake_disconnect,
};

static const net_iface_ops_t s_prio_wifi_ops = {
    .name = "prio_wifi",
    .start = prio_wifi_start,
    .stop = prio_wifi_stop,
    .connect = prio_wifi_connect,
    .disconnect = fake_disconnect,
};

/**
 * @brief The higher route priority wins in either startup order, and a READY callback can switch it.
 *
 * These netifs do not run a DHCP client. With DHCP, esp_netif waits for a real
 * address before it will select the default route, and this test never gets one.
 * Bringing the interface up is enough for the higher route_prio to win.
 */
TEST(net_connect, net_iface_route_priority_and_dynamic_default_switch)
{
    test_case_uses_tcpip();

    esp_netif_driver_ifconfig_t drv_cfg = { 0 };
    drv_cfg.handle = (void *)1;
    drv_cfg.transmit = dummy_netif_transmit;

    esp_netif_inherent_config_t base_eth = { 0 };
    base_eth.if_key = "PRIO_TEST_ETH";
    base_eth.if_desc = "prio_test_eth";
    base_eth.route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO;
    esp_netif_config_t cfg_eth = {
        .base = &base_eth,
        .driver = &drv_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    s_prio_eth_netif = esp_netif_new(&cfg_eth);
    TEST_ASSERT_NOT_NULL(s_prio_eth_netif);

    esp_netif_inherent_config_t base_wifi = { 0 };
    base_wifi.if_key = "PRIO_TEST_WIFI";
    base_wifi.if_desc = "prio_test_wifi";
    base_wifi.route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO;
    esp_netif_config_t cfg_wifi = {
        .base = &base_wifi,
        .driver = &drv_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    s_prio_wifi_netif = esp_netif_new(&cfg_wifi);
    TEST_ASSERT_NOT_NULL(s_prio_wifi_netif);

    net_iface_config_t eth_icfg = { 0 };
    eth_icfg.type = NET_IFACE_TYPE_ETH;
    eth_icfg.desc = "prio_test_eth";
    eth_icfg.route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO;
    eth_icfg.auto_connect_on_start = true;
    eth_icfg.ip.use_dhcp = true;

    net_iface_config_t wifi_icfg = { 0 };
    wifi_icfg.type = NET_IFACE_TYPE_WIFI_STA;
    wifi_icfg.desc = "prio_test_wifi";
    wifi_icfg.route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO;
    wifi_icfg.auto_connect_on_start = true;
    wifi_icfg.ip.use_dhcp = true;

    net_iface_handle_t h_eth = NULL;
    net_iface_handle_t h_wifi = NULL;
    TEST_ESP_OK(net_iface_init_with_ops(&eth_icfg, &s_prio_eth_ops, &h_eth));
    s_test_handle = h_eth;
    TEST_ESP_OK(net_iface_init_with_ops(&wifi_icfg, &s_prio_wifi_ops, &h_wifi));
    s_test_handle2 = h_wifi;

    /* Ethernet (priority 64) comes up first, then Wi-Fi (priority 128). */
    TEST_ESP_OK(net_iface_start(h_eth));
    TEST_ESP_OK(net_iface_wait(h_eth, 500));
    TEST_ASSERT_EQUAL_PTR(s_prio_eth_netif, esp_netif_get_default_netif());

    TEST_ESP_OK(net_iface_start(h_wifi));
    TEST_ESP_OK(net_iface_wait(h_wifi, 500));
    TEST_ASSERT_EQUAL_PTR(s_prio_wifi_netif, esp_netif_get_default_netif());

    TEST_ESP_OK(net_iface_stop(h_wifi));
    TEST_ESP_OK(net_iface_stop(h_eth));

    /* Wi-Fi comes up first. It still wins after Ethernet follows. */
    TEST_ESP_OK(net_iface_start(h_wifi));
    TEST_ESP_OK(net_iface_wait(h_wifi, 500));
    TEST_ASSERT_EQUAL_PTR(s_prio_wifi_netif, esp_netif_get_default_netif());

    TEST_ESP_OK(net_iface_start(h_eth));
    TEST_ESP_OK(net_iface_wait(h_eth, 500));
    TEST_ASSERT_EQUAL_PTR(s_prio_wifi_netif, esp_netif_get_default_netif());

    /* A READY callback raises Ethernet above Wi-Fi and asks esp_netif to pick again. */
    s_boost_eth_prio_on_ready = true;
    TEST_ESP_OK(net_iface_register_event_handler(on_ready_boost_eth_prio, NULL));
    s_prio_handler_registered = true;

    TEST_ESP_OK(net_iface_stop(h_eth));
    TEST_ESP_OK(net_iface_start(h_eth));
    TEST_ESP_OK(net_iface_wait(h_eth, 500));
    vTaskDelay(pdMS_TO_TICKS(30));
    TEST_ASSERT_EQUAL_PTR(s_prio_eth_netif, esp_netif_get_default_netif());

    test_clear_prio_handler();

    TEST_ESP_OK(net_iface_stop(h_wifi));
    TEST_ESP_OK(net_iface_stop(h_eth));
    TEST_ESP_OK(net_iface_deinit(h_wifi));
    TEST_ESP_OK(net_iface_deinit(h_eth));
    s_test_handle = NULL;
    s_test_handle2 = NULL;

    esp_netif_destroy(s_prio_wifi_netif);
    s_prio_wifi_netif = NULL;
    esp_netif_destroy(s_prio_eth_netif);
    s_prio_eth_netif = NULL;

    /*
     * test_case_uses_tcpip() moved the leak baseline to after LwIP startup,
     * while the fixture netifs from TEST_SETUP are still allocated. Check now.
     * Tear-down frees those fixture netifs, so its own check sees free memory go up.
     */
    vTaskDelay(pdMS_TO_TICKS(20));
    test_utils_finish_and_evaluate_leaks(0, 0);
}

#if CONFIG_NET_CONNECT_WIFI
static esp_err_t homehub_wifi_connect(net_iface_handle_t self)
{
    /* An empty name is not a failure. The interface stays started. */
    if (self->cfg.wifi_sta.ssid[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    net_iface_notify_connected(self);
    return ESP_OK;
}

static const net_iface_ops_t s_homehub_wifi_ops = {
    .name = "hub_wifi",
    .start = fake_start,
    .stop = fake_stop,
    .connect = homehub_wifi_connect,
    .disconnect = fake_disconnect,
};
#endif

/**
 * @brief Required Ethernet becomes ready while unprovisioned Wi-Fi stays started.
 *
 * Credentials stored later bring Wi-Fi to ready as well.
 */
TEST(net_connect, net_iface_home_hub_required_eth_and_unprovisioned_wifi)
{
#if CONFIG_NET_CONNECT_WIFI
    net_iface_config_t eth_cfg = { 0 };
    eth_cfg.type = NET_IFACE_TYPE_ETH;
    eth_cfg.desc = TEST_NETIF_DESC_ETH;
    eth_cfg.route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO;
    eth_cfg.required = true;
    eth_cfg.auto_connect_on_start = true;
    eth_cfg.ip.use_dhcp = true;

    net_iface_config_t wifi_cfg = { 0 };
    wifi_cfg.type = NET_IFACE_TYPE_WIFI_STA;
    wifi_cfg.desc = TEST_NETIF_DESC_STA;
    wifi_cfg.route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO;
    wifi_cfg.required = false;
    /* Auto-connect is on, but the empty name keeps Wi-Fi in STARTED. */
    wifi_cfg.auto_connect_on_start = true;
    wifi_cfg.ip.use_dhcp = true;
    wifi_cfg.wifi_sta.ssid[0] = '\0';

    net_iface_handle_t h_eth = NULL;
    net_iface_handle_t h_wifi = NULL;
    TEST_ESP_OK(net_iface_init_with_ops(&eth_cfg, &s_slow_eth_ops, &h_eth));
    s_test_handle = h_eth;
    TEST_ESP_OK(net_iface_init_with_ops(&wifi_cfg, &s_homehub_wifi_ops, &h_wifi));
    s_test_handle2 = h_wifi;

    TEST_ESP_OK(net_iface_start(h_eth));
    TEST_ESP_OK(net_iface_start(h_wifi));

    TEST_ESP_OK(net_iface_wait_all(&h_eth, 1, 1000));
    wait_for_startup_done(h_wifi);

    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h_eth));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_STARTED, net_iface_get_state(h_wifi));

    TEST_ESP_OK(net_wifi_sta_set_credentials(h_wifi, "HomeHubWiFi", "SecretPass123"));
    TEST_ESP_OK(net_iface_connect(h_wifi));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTED, net_iface_get_state(h_wifi));

    ip_event_got_ip_t got_ip = {
        .esp_netif = test_netif_sta,
    };
    TEST_ESP_OK(esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip, sizeof(got_ip), pdMS_TO_TICKS(100)));
    TEST_ESP_OK(net_iface_wait(h_wifi, 500));

    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h_wifi));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_READY, net_iface_get_state(h_eth));

    TEST_ESP_OK(net_iface_stop(h_wifi));
    TEST_ESP_OK(net_iface_stop(h_eth));
    TEST_ESP_OK(net_iface_deinit(h_wifi));
    TEST_ESP_OK(net_iface_deinit(h_eth));
    s_test_handle = NULL;
    s_test_handle2 = NULL;
#else
    TEST_IGNORE_MESSAGE("Wi-Fi is turned off in this build");
#endif
}

/**
 * @brief An address event that arrives after stop() does not mark the interface ready.
 */
TEST(net_connect, net_iface_stale_ip_event_after_stop_is_ignored)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(test_register_handler());
    TEST_ESP_OK(open_fake_iface(&h, false));

    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);
    TEST_ESP_OK(net_iface_connect(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_CONNECTED, net_iface_get_state(h));

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    vTaskDelay(pdMS_TO_TICKS(20));
    test_reset_events();

    ip_event_got_ip_t stale_ip = {
        .esp_netif = test_netif_sta,
    };
    TEST_ESP_OK(esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &stale_ip, sizeof(stale_ip), pdMS_TO_TICKS(100)));
    vTaskDelay(pdMS_TO_TICKS(30));

    TEST_ASSERT_EQUAL(NET_IFACE_STATE_INIT, net_iface_get_state(h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, net_iface_wait(h, 20));
    TEST_ASSERT_EQUAL(0, s_event_count);

    test_unregister_handler();
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
}

/**
 * @brief The startup task keeps at least 512 bytes of stack free.
 */
TEST(net_connect, net_iface_startup_task_stack_high_water_mark)
{
    net_iface_handle_t h = NULL;
    TEST_ESP_OK(open_fake_iface(&h, false));

    TEST_ESP_OK(net_iface_start(h));
    wait_for_startup_done(h);

    ESP_LOGI("net_connect_test", "iface_startup_task stack high-water mark: %u free bytes (of %d configured)",
             (unsigned)h->startup_stack_hwm, CONFIG_NET_CONNECT_STARTUP_TASK_STACK_SIZE);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(512, h->startup_stack_hwm);

    TEST_ESP_OK(net_iface_stop(h));
    TEST_ESP_OK(net_iface_deinit(h));
    s_test_handle = NULL;
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
    RUN_TEST_CASE(net_connect, net_iface_lifecycle_state_transitions);
    RUN_TEST_CASE(net_connect, net_iface_event_callbacks_and_order);
    RUN_TEST_CASE(net_connect, net_iface_failure_records_last_error);
    RUN_TEST_CASE(net_connect, net_iface_late_callback_discovers_state);
    RUN_TEST_CASE(net_connect, net_iface_ip_event_drives_ready_and_lost_ip);
    RUN_TEST_CASE(net_connect, net_iface_start_returns_promptly_while_driver_blocks);
    RUN_TEST_CASE(net_connect, net_iface_parallel_startup_no_head_of_line_blocking);
    RUN_TEST_CASE(net_connect, net_iface_async_startup_failure_notifies_waiter_and_event);
    RUN_TEST_CASE(net_connect, net_iface_start_task_alloc_failure_returns_no_mem);
    RUN_TEST_CASE(net_connect, net_wifi_sta_create_defaults_and_custom_params);
    RUN_TEST_CASE(net_connect, net_wifi_sta_set_credentials_validation);
    RUN_TEST_CASE(net_connect, net_eth_thread_ppp_create_and_deinit);
    RUN_TEST_CASE(net_connect, net_iface_wait_immediate_on_ready_and_failed);
    RUN_TEST_CASE(net_connect, net_iface_wait_boundary_race_no_lost_wakeup);
    RUN_TEST_CASE(net_connect, net_iface_wait_timeout_leaves_connection_running);
    RUN_TEST_CASE(net_connect, net_iface_wait_all_shared_budget_and_fail_fast);
    RUN_TEST_CASE(net_connect, net_iface_wait_any_first_ready_and_all_failed);
    RUN_TEST_CASE(net_connect, net_iface_wait_from_event_loop_returns_invalid_state);
    RUN_TEST_CASE(net_connect, net_iface_stop_during_starting_cancels_cleanly);
    RUN_TEST_CASE(net_connect, net_iface_repeated_start_stop_deinit_cycles);
    RUN_TEST_CASE(net_connect, net_iface_restart_after_failed_start_succeeds);
    RUN_TEST_CASE(net_connect, net_iface_four_interfaces_concurrent_events_and_waiters);
    RUN_TEST_CASE(net_connect, net_iface_home_hub_required_eth_and_unprovisioned_wifi);
    RUN_TEST_CASE(net_connect, net_iface_stale_ip_event_after_stop_is_ignored);
    RUN_TEST_CASE(net_connect, net_iface_startup_task_stack_high_water_mark);
    /* Last: this test starts LwIP, which stays running until reset. */
    RUN_TEST_CASE(net_connect, net_iface_route_priority_and_dynamic_default_switch);
}

void app_main(void)
{
    UNITY_MAIN(net_connect);
}
