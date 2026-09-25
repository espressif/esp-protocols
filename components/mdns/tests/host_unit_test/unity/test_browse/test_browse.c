/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>

#include "mdns.h"
#include "mdns_browser.h"
#include "mdns_cache.h"
#include "mdns_mem_caps.h"
#include "mdns_private.h"
#include "mock_mdns_pcb.h"
#include "unity.h"

#define INSTANCE    "cached-service"
#define SERVICE     "_cachetest"
#define PROTO       "_tcp"
#define HOSTNAME    "cache-host"
#define PORT        8080
#define TTL         120

#define TXT_VAL     "value"
#define TXT_VAL_LEN 5

static uint8_t s_netif_storage;
static const mdns_txt_linked_item_t s_txt = {
    .key = "key",
    .value = TXT_VAL,
    .value_len = TXT_VAL_LEN,
    .next = NULL,
};
static const esp_ip_addr_t s_addr = ESP_IP4ADDR_INIT(192, 168, 1, 100);

static size_t s_notify_calls;
static uint32_t s_last_ttl;

static esp_netif_t *test_netif(void)
{
    return (esp_netif_t *)&s_netif_storage;
}

static void browse_callback(mdns_result_t *result)
{
    // Results belong to the browser and are only valid during this callback.
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_NULL(result->next);
    TEST_ASSERT_EQUAL_PTR(test_netif(), result->esp_netif);
    TEST_ASSERT_EQUAL(MDNS_IP_PROTOCOL_V4, result->ip_protocol);
    TEST_ASSERT_EQUAL_STRING(INSTANCE, result->instance_name);
    TEST_ASSERT_EQUAL_STRING(SERVICE, result->service_type);
    TEST_ASSERT_EQUAL_STRING(PROTO, result->proto);
    TEST_ASSERT_EQUAL_STRING(HOSTNAME, result->hostname);
    TEST_ASSERT_EQUAL_UINT16(PORT, result->port);
    TEST_ASSERT_NOT_NULL(result->txt);
    TEST_ASSERT_EQUAL_UINT8(TXT_VAL_LEN, result->txt_value_len[0]);
    TEST_ASSERT_EQUAL_size_t(1, result->txt_count);
    TEST_ASSERT_NOT_NULL(result->addr);
    TEST_ASSERT_EQUAL(ESP_IPADDR_TYPE_V4, result->addr->addr.type);
    TEST_ASSERT_EQUAL_UINT32(s_addr.u_addr.ip4.addr, result->addr->addr.u_addr.ip4.addr);

    s_notify_calls++;
    s_last_ttl = result->ttl;
}

static void unexpected_callback(mdns_result_t *result)
{
    (void)result;
    TEST_FAIL_MESSAGE("A non-matching browse received a notification");
}

static mdns_txt_linked_item_t *clone_txt(const mdns_txt_linked_item_t *src_txt)
{
    mdns_txt_linked_item_t *item = mdns_mem_calloc(1, sizeof(mdns_txt_linked_item_t));
    TEST_ASSERT_NOT_NULL(item);

    item->key = mdns_mem_strdup(src_txt->key);
    TEST_ASSERT_NOT_NULL(item->key);

    item->value_len = src_txt->value_len;

    if (src_txt->value_len > 0) {
        item->value = mdns_mem_malloc(src_txt->value_len);
        TEST_ASSERT_NOT_NULL(item->value);
        memcpy(item->value, src_txt->value, src_txt->value_len);
    }

    return item;
}

static void cache_service_details(void)
{
    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_srv(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                       HOSTNAME, INSTANCE, SERVICE, PROTO,
                                                                       0, 0, PORT, TTL));
    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_txt(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                       INSTANCE, SERVICE, PROTO, clone_txt(&s_txt), TTL));
    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_addr(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                        HOSTNAME, &s_addr, TTL));
}

static void cache_service(void)
{
    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_ptr(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                       INSTANCE, SERVICE, PROTO, TTL));
    cache_service_details();
}

void mdns_test_set_up(void)
{
    mdns_priv_browse_free();
    mdns_priv_cache_clear();
    s_notify_calls = 0;
    s_last_ttl = 0;
}

void mdns_test_tear_down(void)
{
    mdns_priv_browse_free();
    mdns_priv_cache_clear();
}

void setup_cmock(void)
{
    mdns_priv_probe_all_pcbs_CMockIgnore();
    mdns_priv_pcb_announce_CMockIgnore();
    mdns_priv_pcb_send_bye_service_CMockIgnore();
    mdns_priv_pcb_check_probing_services_CMockIgnore();
    mdns_priv_pcb_is_after_probing_IgnoreAndReturn(true);
    mdsn_priv_pcb_is_inited_IgnoreAndReturn(false);
}

static void test_browse_add_delete_sequence(void)
{
    const char *other_service = "_other";

    // Register A and reject an immediate duplicate.
    TEST_ASSERT_NOT_NULL(mdns_browse_new(SERVICE, PROTO, browse_callback));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(SERVICE, PROTO));

    TEST_ASSERT_NULL(mdns_browse_new(SERVICE, PROTO, browse_callback));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(SERVICE, PROTO));

    // Register B without affecting A.
    TEST_ASSERT_NOT_NULL(mdns_browse_new(other_service, PROTO, unexpected_callback));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(SERVICE, PROTO));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(other_service, PROTO));

    // Delete A. B remains registered, and deleting A again must fail.
    TEST_ASSERT_EQUAL(ESP_OK, mdns_browse_delete(SERVICE, PROTO));
    TEST_ASSERT_FALSE(mdns_priv_browse_has_service(SERVICE, PROTO));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(other_service, PROTO));

    TEST_ASSERT_EQUAL(ESP_FAIL, mdns_browse_delete(SERVICE, PROTO));

    // A can be registered again immediately after deletion.
    TEST_ASSERT_NOT_NULL(mdns_browse_new(SERVICE, PROTO, browse_callback));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(SERVICE, PROTO));
    TEST_ASSERT_TRUE(mdns_priv_browse_has_service(other_service, PROTO));

    TEST_ASSERT_EQUAL(ESP_OK, mdns_browse_delete(SERVICE, PROTO));
    TEST_ASSERT_EQUAL(ESP_OK, mdns_browse_delete(other_service, PROTO));
    TEST_ASSERT_FALSE(mdns_priv_browse_has_service(SERVICE, PROTO));
    TEST_ASSERT_FALSE(mdns_priv_browse_has_service(other_service, PROTO));
}

static void test_browse_cached_result_and_delete(void)
{
    // Consume pending updates before registering the browse.
    // The initial callback must come from replaying the cache on browse start.
    cache_service();
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(0, s_notify_calls);

    TEST_ASSERT_NOT_NULL(mdns_browse_new(SERVICE, PROTO, browse_callback));
    TEST_ASSERT_EQUAL_size_t(1, s_notify_calls);
    TEST_ASSERT_EQUAL_UINT32(TTL, s_last_ttl);

    TEST_ASSERT_EQUAL(ESP_OK, mdns_browse_delete(SERVICE, PROTO));
    TEST_ASSERT_FALSE(mdns_priv_browse_has_service(SERVICE, PROTO));

    // Further cache changes must not call the deleted browse notifier.
    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_srv(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                       HOSTNAME, INSTANCE, SERVICE, PROTO,
                                                                       0, 0, PORT + 1, TTL));
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(1, s_notify_calls);
}

static void test_browse_matches_service_and_requires_ptr(void)
{
    TEST_ASSERT_NOT_NULL(mdns_browse_new(SERVICE, PROTO, browse_callback));
    TEST_ASSERT_NOT_NULL(mdns_browse_new("_other", PROTO, unexpected_callback));
    TEST_ASSERT_NOT_NULL(mdns_browse_new(SERVICE, "_udp", unexpected_callback));

    // SRV, TXT and addresses alone do not establish a browse result.
    cache_service_details();
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(0, s_notify_calls);

    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_ptr(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                       INSTANCE, SERVICE, PROTO, TTL));
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(1, s_notify_calls);
    TEST_ASSERT_EQUAL_UINT32(TTL, s_last_ttl);
}

static void test_browse_goodbye_and_rediscovery(void)
{
    TEST_ASSERT_NOT_NULL(mdns_browse_new(SERVICE, PROTO, browse_callback));

    cache_service();
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(1, s_notify_calls);
    TEST_ASSERT_EQUAL_UINT32(TTL, s_last_ttl);

    // Cache goodbye should notify the browse.
    TEST_ASSERT_NOT_EQUAL(MDNS_CACHE_ERROR, mdns_priv_cache_update_ptr(test_netif(), MDNS_IP_PROTOCOL_V4,
                                                                       INSTANCE, SERVICE, PROTO, 0));
    mdns_priv_cache_remove_expired_records(esp_timer_get_time());
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(2, s_notify_calls);
    TEST_ASSERT_EQUAL_UINT32(0, s_last_ttl);

    // The same browse remains active and can discover the service again.
    cache_service();
    mdns_priv_cache_process_sync();
    TEST_ASSERT_EQUAL_size_t(3, s_notify_calls);
    TEST_ASSERT_EQUAL_UINT32(TTL, s_last_ttl);
}

void run_unity_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_browse_add_delete_sequence);
    RUN_TEST(test_browse_cached_result_and_delete);
    RUN_TEST(test_browse_matches_service_and_requires_ptr);
    RUN_TEST(test_browse_goodbye_and_rediscovery);
    UNITY_END();
}
