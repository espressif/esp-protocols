/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "eppp_link.h"
#include "lwip/sockets.h"
#include "esp_log.h"
#include "ping/ping_sock.h"
#include "driver/uart.h"
#include "test_utils.h"
#include "unity.h"
#include "test_utils.h"
#include "unity_fixture.h"
#include "memory_checks.h"

static const char *TAG = "eppp_test_app";

TEST_GROUP(eppp_test);
TEST_SETUP(eppp_test)
{
    test_utils_record_free_mem();
    TEST_ESP_OK(test_utils_set_leak_level(0, ESP_LEAK_TYPE_CRITICAL, ESP_COMP_LEAK_GENERAL));
}

TEST_TEAR_DOWN(eppp_test)
{
    test_utils_finish_and_evaluate_leaks(32, 64);
}
static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%" PRId32 "bytes from %s icmp_seq=%d ttl=%d time=%" PRId32 " ms\n",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%" PRId32 " packets transmitted, %" PRId32 " received, time %" PRId32 "ms\n", transmitted, received, total_time_ms);

}

static void client_task(void *ctx)
{
    eppp_config_t *client_config = ctx;
    esp_netif_t *eppp_client = eppp_connect(client_config);

    if (eppp_client == NULL) {
        ESP_LOGE(TAG, "Failed to connect");
    }

    vTaskDelete(NULL);
}

void app_main2(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());


    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Sets up the default EPPP-connection
     */
    uint32_t server_ip = ESP_IP4TOADDR(192, 168, 11, 1);
    uint32_t client_ip = ESP_IP4TOADDR(192, 168, 11, 2);
    eppp_config_t server_config = {
        .uart = {
            .port = UART_NUM_1,
            .baud = 921600,
            .tx_io = 25,
            .rx_io = 26,
            .queue_size = 16,
            .rx_buffer_size = 1024,
        },
        . task = {
            .run_task = true,
            .stack_size = 4096,
            .priority = 18,
        },
        . ppp = {
            .our_ip4_addr = server_ip,
            .their_ip4_addr = client_ip,
        }
    };
    eppp_config_t client_config = EPPP_DEFAULT_CLIENT_CONFIG();

    memcpy(&client_config, &server_config, sizeof(server_config));
    client_config.uart.port = UART_NUM_2;
    client_config.uart.tx_io = 4;
    client_config.uart.rx_io = 5;
    client_config.ppp.our_ip4_addr = client_ip;
    client_config.ppp.their_ip4_addr = server_ip;
    xTaskCreate(client_task, "client_task", 4096, &client_config, 5, NULL);

    esp_netif_t *eppp_server = eppp_listen(&server_config);
    if (eppp_server == NULL) {
        ESP_LOGE(TAG, "Failed to setup connection");
        return ;
    }

    // Try to ping client's IP on server interface, so the packets go over the wires
    // (if we didn't set the interface, ping would run locally on the client's netif)
    ip_addr_t target_addr = { .type = IPADDR_TYPE_V4, .u_addr.ip4.addr = client_ip };

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.timeout_ms = 2000;
    ping_config.interval_ms = 1000,
    ping_config.target_addr = target_addr;
    ping_config.count = 100;
    ping_config.interface = esp_netif_get_netif_impl_index(eppp_server);
    /* set callback functions */
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end = test_on_ping_end;
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    /* start ping */
    esp_ping_start(ping);

}

static void open_client_task(void *ctx)
{
    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
    config.uart.port = UART_NUM_2;
    config.uart.tx_io = 4;
    config.uart.rx_io = 5;

    esp_netif_t **eppp_client = ctx;
    *eppp_client = eppp_connect(&config);
    vTaskDelete(NULL);
}

TEST(eppp_test, open_close)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Need to connect the client in a separate thread, as the simplified API blocks until connection
    esp_netif_t *eppp_client = NULL;
    xTaskCreate(open_client_task, "client_task", 4096, &eppp_client, 5, NULL);
    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
    esp_netif_t *eppp_server = eppp_listen(&config);
    TEST_ASSERT_NOT_NULL(eppp_server);
    TEST_ASSERT_NOT_NULL(eppp_client);


}

TEST_GROUP_RUNNER(eppp_test)
{
    RUN_TEST_CASE(eppp_test, open_close)
}

void app_main(void)
{
    UNITY_MAIN(eppp_test);
}
