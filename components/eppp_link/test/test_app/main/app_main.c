/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
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
#include "lwip/sys.h"

#define CLIENT_INFO_CONNECTED   BIT0
#define CLIENT_INFO_DISCONNECT  BIT1
#define CLIENT_INFO_CLOSED      BIT2
#define PING_SUCCEEDED          BIT3
#define PING_FAILED             BIT4
#define STOP_WORKER_TASK        BIT5
#define WORKER_TASK_STOPPED     BIT6

TEST_GROUP(eppp_test);
TEST_SETUP(eppp_test)
{
    // Perform some open/close operations to disregard lazy init one-time allocations
    // LWIP: core protection mutex
    sys_arch_protect();
    sys_arch_unprotect(0);
    // UART: install and delete both drivers to disregard potential leak in allocated interrupt slot
    TEST_ESP_OK(uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0));
    TEST_ESP_OK(uart_driver_delete(UART_NUM_1));
    TEST_ESP_OK(uart_driver_install(UART_NUM_2, 256, 0, 0, NULL, 0));
    TEST_ESP_OK(uart_driver_delete(UART_NUM_2));
    // PING: used for timestamps
    struct timeval time;
    gettimeofday(&time, NULL);

    test_utils_record_free_mem();
    TEST_ESP_OK(test_utils_set_leak_level(0, ESP_LEAK_TYPE_CRITICAL, ESP_COMP_LEAK_GENERAL));
}

TEST_TEAR_DOWN(eppp_test)
{
    test_utils_finish_and_evaluate_leaks(32, 64);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    EventGroupHandle_t event = args;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%" PRId32 " packets transmitted, %" PRId32 " received, time %" PRId32 "ms\n", transmitted, received, total_time_ms);
    if (transmitted == received) {
        xEventGroupSetBits(event, PING_SUCCEEDED);
    } else {
        xEventGroupSetBits(event, PING_FAILED);
    }
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

struct client_info {
    esp_netif_t *netif;
    EventGroupHandle_t event;
};

static void open_client_task(void *ctx)
{
    struct client_info *info = ctx;
    eppp_config_t config = EPPP_DEFAULT_CLIENT_CONFIG();
    config.uart.port = UART_NUM_2;
    config.uart.tx_io = 4;
    config.uart.rx_io = 5;

    info->netif = eppp_connect(&config);
    xEventGroupSetBits(info->event, CLIENT_INFO_CONNECTED);

    // wait for disconnection trigger
    EventBits_t bits = xEventGroupWaitBits(info->event, CLIENT_INFO_DISCONNECT, pdFALSE, pdFALSE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & CLIENT_INFO_DISCONNECT, CLIENT_INFO_DISCONNECT);
    eppp_close(info->netif);
    xEventGroupSetBits(info->event, CLIENT_INFO_CLOSED);
    vTaskDelete(NULL);
}

TEST(eppp_test, init_deinit)
{
    // Init and deinit server size
    eppp_config_t config = EPPP_DEFAULT_CONFIG(0, 0);
    esp_netif_t *netif = eppp_init(EPPP_SERVER, &config);
    TEST_ASSERT_NOT_NULL(netif);
    eppp_deinit(netif);
    netif = NULL;
    // Init and deinit client size
    netif = eppp_init(EPPP_CLIENT, &config);
    TEST_ASSERT_NOT_NULL(netif);
    eppp_deinit(netif);
}

static EventBits_t ping_test(uint32_t addr, esp_netif_t *netif, EventGroupHandle_t event)
{
    ip_addr_t target_addr = { .type = IPADDR_TYPE_V4, .u_addr.ip4.addr = addr };
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.interval_ms = 100;
    ping_config.target_addr = target_addr;
    ping_config.interface = esp_netif_get_netif_impl_index(netif);
    esp_ping_callbacks_t cbs = { .cb_args = event, .on_ping_end = test_on_ping_end, .on_ping_success = test_on_ping_success };
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);
    esp_ping_start(ping);
    // Wait for the client thread closure and delete locally created objects
    EventBits_t bits = xEventGroupWaitBits(event, PING_SUCCEEDED | PING_FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & (PING_SUCCEEDED | PING_FAILED), PING_SUCCEEDED);
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    return bits;
}

TEST(eppp_test, open_close)
{
    test_case_uses_tcpip();

    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
    struct client_info client = { .netif = NULL, .event =  xEventGroupCreate()};

    TEST_ESP_OK(esp_event_loop_create_default());

    TEST_ASSERT_NOT_NULL(client.event);

    // Need to connect the client in a separate thread, as the simplified API blocks until connection
    xTaskCreate(open_client_task, "client_task", 4096, &client, 5, NULL);

    // Now start the server
    esp_netif_t *eppp_server = eppp_listen(&config);

    // Wait for the client to connect
    EventBits_t bits = xEventGroupWaitBits(client.event, CLIENT_INFO_CONNECTED, pdFALSE, pdFALSE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & CLIENT_INFO_CONNECTED, CLIENT_INFO_CONNECTED);

    // Check that both server and client are valid netif pointers
    TEST_ASSERT_NOT_NULL(eppp_server);
    TEST_ASSERT_NOT_NULL(client.netif);

    // Now that we're connected, let's try to ping clients address
    bits = ping_test(config.ppp.their_ip4_addr.addr, eppp_server, client.event);
    TEST_ASSERT_EQUAL(bits & (PING_SUCCEEDED | PING_FAILED), PING_SUCCEEDED);

    // Trigger client disconnection and close the server
    xEventGroupSetBits(client.event, CLIENT_INFO_DISCONNECT);
    eppp_close(eppp_server);

    // Wait for the client thread closure and delete locally created objects
    bits = xEventGroupWaitBits(client.event, CLIENT_INFO_CLOSED, pdFALSE, pdFALSE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & CLIENT_INFO_CLOSED, CLIENT_INFO_CLOSED);

    TEST_ESP_OK(esp_event_loop_delete_default());
    vEventGroupDelete(client.event);

    // wait for the lwip sockets to close cleanly
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void on_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    EventGroupHandle_t event = arg;
    if (base == IP_EVENT && event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        esp_netif_t *netif = e->esp_netif;
        ESP_LOGI("test", "Got IPv4 event: Interface \"%s(%s)\" address: " IPSTR, esp_netif_get_desc(netif),
                 esp_netif_get_ifkey(netif), IP2STR(&e->ip_info.ip));
        if (strcmp("pppos_server", esp_netif_get_desc(netif)) == 0) {
            xEventGroupSetBits(event, 1 << EPPP_SERVER);
        } else if (strcmp("pppos_client", esp_netif_get_desc(netif)) == 0) {
            xEventGroupSetBits(event, 1 << EPPP_CLIENT);
        }
    } else if (base == NETIF_PPP_STATUS && event_id == NETIF_PPP_ERRORUSER) {
        esp_netif_t **netif = data;
        ESP_LOGI("test", "Disconnected interface \"%s(%s)\"", esp_netif_get_desc(*netif), esp_netif_get_ifkey(*netif));
        if (strcmp("pppos_server", esp_netif_get_desc(*netif)) == 0) {
            xEventGroupSetBits(event, 1 << EPPP_SERVER);
        } else if (strcmp("pppos_client", esp_netif_get_desc(*netif)) == 0) {
            xEventGroupSetBits(event, 1 << EPPP_CLIENT);
        }
    }
}

TEST(eppp_test, open_close_nonblocking)
{
    test_case_uses_tcpip();
    EventGroupHandle_t event = xEventGroupCreate();

    eppp_config_t server_config = EPPP_DEFAULT_SERVER_CONFIG();
    TEST_ESP_OK(esp_event_loop_create_default());

    // Open the server size
    TEST_ESP_OK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, event));
    esp_netif_t *eppp_server = eppp_open(EPPP_SERVER, &server_config, 0);
    TEST_ASSERT_NOT_NULL(eppp_server);
    // Open the client size
    eppp_config_t client_config = EPPP_DEFAULT_SERVER_CONFIG();
    client_config.uart.port = UART_NUM_2;
    client_config.uart.tx_io = 4;
    client_config.uart.rx_io = 5;
    esp_netif_t *eppp_client = eppp_open(EPPP_CLIENT, &client_config, 0);
    TEST_ASSERT_NOT_NULL(eppp_client);
    const EventBits_t wait_bits = (1 << EPPP_SERVER) | (1 << EPPP_CLIENT);
    EventBits_t bits = xEventGroupWaitBits(event, wait_bits, pdTRUE, pdTRUE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & wait_bits, wait_bits);

    // Now that we're connected, let's try to ping clients address
    bits = ping_test(server_config.ppp.their_ip4_addr.addr, eppp_server, event);
    TEST_ASSERT_EQUAL(bits & (PING_SUCCEEDED | PING_FAILED), PING_SUCCEEDED);

    // stop network for both client and server
    eppp_netif_stop(eppp_client, 0); // ignore result, since we're not waiting for clean close
    eppp_close(eppp_server);
    eppp_close(eppp_client);         // finish client close
    TEST_ESP_OK(esp_event_loop_delete_default());
    vEventGroupDelete(event);

    // wait for the lwip sockets to close cleanly
    vTaskDelay(pdMS_TO_TICKS(1000));
}


struct worker {
    esp_netif_t *eppp_server;
    esp_netif_t *eppp_client;
    EventGroupHandle_t event;
};

static void worker_task(void *ctx)
{
    struct worker *info = ctx;
    while (1) {
        eppp_perform(info->eppp_server);
        eppp_perform(info->eppp_client);
        if (xEventGroupGetBits(info->event) & STOP_WORKER_TASK) {
            break;
        }
    }
    xEventGroupSetBits(info->event, WORKER_TASK_STOPPED);
    vTaskDelete(NULL);
}

TEST(eppp_test, open_close_taskless)
{
    test_case_uses_tcpip();
    struct worker info = { .event = xEventGroupCreate() };

    TEST_ESP_OK(esp_event_loop_create_default());
    TEST_ESP_OK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, info.event));
    TEST_ESP_OK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_event, info.event));

    // Create server
    eppp_config_t server_config = EPPP_DEFAULT_SERVER_CONFIG();
    info.eppp_server = eppp_init(EPPP_SERVER, &server_config);
    TEST_ASSERT_NOT_NULL(info.eppp_server);
    // Create client
    eppp_config_t client_config = EPPP_DEFAULT_CLIENT_CONFIG();
    client_config.uart.port = UART_NUM_2;
    client_config.uart.tx_io = 4;
    client_config.uart.rx_io = 5;
    info.eppp_client = eppp_init(EPPP_CLIENT, &client_config);
    TEST_ASSERT_NOT_NULL(info.eppp_client);
    // Start workers
    xTaskCreate(worker_task, "worker", 4096, &info, 5, NULL);
    // Start network
    TEST_ESP_OK(eppp_netif_start(info.eppp_server));
    TEST_ESP_OK(eppp_netif_start(info.eppp_client));

    const EventBits_t wait_bits = (1 << EPPP_SERVER) | (1 << EPPP_CLIENT);
    EventBits_t bits = xEventGroupWaitBits(info.event, wait_bits, pdTRUE, pdTRUE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & wait_bits, wait_bits);
    xEventGroupClearBits(info.event, wait_bits);

    // Now that we're connected, let's try to ping clients address
    bits = ping_test(server_config.ppp.their_ip4_addr.addr, info.eppp_server, info.event);
    TEST_ASSERT_EQUAL(bits & (PING_SUCCEEDED | PING_FAILED), PING_SUCCEEDED);

    // stop network for both client and server, we won't wait for completion so expecting ESP_FAIL
    TEST_ASSERT_EQUAL(eppp_netif_stop(info.eppp_client, 0), ESP_FAIL);
    TEST_ASSERT_EQUAL(eppp_netif_stop(info.eppp_server, 0), ESP_FAIL);
    // and wait for completion
    bits = xEventGroupWaitBits(info.event, wait_bits, pdTRUE, pdTRUE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & wait_bits, wait_bits);

    // now stop the worker
    xEventGroupSetBits(info.event, STOP_WORKER_TASK);
    bits = xEventGroupWaitBits(info.event, WORKER_TASK_STOPPED, pdTRUE, pdTRUE, pdMS_TO_TICKS(50000));
    TEST_ASSERT_EQUAL(bits & WORKER_TASK_STOPPED, WORKER_TASK_STOPPED);

    // and destroy objects
    eppp_deinit(info.eppp_server);
    eppp_deinit(info.eppp_client);
    TEST_ESP_OK(esp_event_loop_delete_default());
    vEventGroupDelete(info.event);

    // wait for the lwip sockets to close cleanly
    vTaskDelay(pdMS_TO_TICKS(1000));
}


TEST_GROUP_RUNNER(eppp_test)
{
    RUN_TEST_CASE(eppp_test, init_deinit)
    RUN_TEST_CASE(eppp_test, open_close)
    RUN_TEST_CASE(eppp_test, open_close_nonblocking)
    RUN_TEST_CASE(eppp_test, open_close_taskless)
}

void app_main(void)
{
    UNITY_MAIN(eppp_test);
}
