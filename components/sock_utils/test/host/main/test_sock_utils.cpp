/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include <strings.h>
#include <poll.h>
#include "gethostname.h"
#include "ifaddrs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_session.hpp"

#define TEST_PORT_NUMBER 3333
#define TEST_PORT_STRING "3333"

static char buffer[64];

static esp_err_t dummy_transmit(void *h, void *buffer, size_t len)
{
    return ESP_OK;
}

static esp_err_t dummy_transmit_wrap(void *h, void *buffer, size_t len, void *pbuf)
{
    return ESP_OK;
}

static esp_netif_t *create_test_netif(const char *if_key, uint8_t last_octet)
{
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_ip_info_t ip = { };
    ip.ip.addr = ESP_IP4TOADDR(1, 2, 3, last_octet);
    base_cfg.ip_info = &ip;
    base_cfg.if_key = if_key;
    // create a dummy driver, so the netif could be started and set up
    base_cfg.flags = ESP_NETIF_FLAG_AUTOUP;
    esp_netif_driver_ifconfig_t driver_cfg = {};
    driver_cfg.handle = (esp_netif_iodriver_handle)1;
    driver_cfg.transmit = dummy_transmit;
    driver_cfg.transmit_wrap = dummy_transmit_wrap;
    esp_netif_config_t cfg = { .base = &base_cfg, .driver = &driver_cfg, .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA };
    esp_netif_t *netif = esp_netif_new(&cfg);
    // need to start the netif to be considered in tests
    esp_netif_action_start(netif, nullptr, 0, nullptr);
    return netif;
}

TEST_CASE("esp_getnameinfo() for IPv4", "[sock_utils]")
{
    struct sockaddr_in sock_addr = {};
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(TEST_PORT_NUMBER); // sock_addr fields are in network byte order
    REQUIRE(esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) == 0);
    CHECK(strcmp("0.0.0.0", buffer) == 0);
    CHECK(esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), NULL, 0, buffer, sizeof(buffer), NI_NUMERICSERV) == 0);
    CHECK(strcmp(TEST_PORT_STRING, buffer) == 0);
}

TEST_CASE("esp_getnameinfo() for IPv6", "[sock_utils]")
{
    struct sockaddr_in6 sock_addr = {};
    sock_addr.sin6_family = AF_INET6;
    sock_addr.sin6_port = htons(TEST_PORT_NUMBER);
    REQUIRE(inet_pton(AF_INET6, "2001:db8::1", &sock_addr.sin6_addr) == 1);
    // lwIP prints IPv6 addresses in upper case
    REQUIRE(esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) == 0);
    CHECK(strcasecmp("2001:db8::1", buffer) == 0);
    CHECK(esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), NULL, 0, buffer, sizeof(buffer), NI_NUMERICSERV) == 0);
    CHECK(strcmp(TEST_PORT_STRING, buffer) == 0);
    char too_short[8];
    CHECK(esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), too_short, sizeof(too_short), NULL, 0, NI_NUMERICHOST) != 0);

    // A scoped (link-local) address gets the interface name appended, like glibc, or the index if there is no such interface
    REQUIRE(inet_pton(AF_INET6, "fe80::1", &sock_addr.sin6_addr) == 1);
    esp_netif_t *esp_netif = create_test_netif("station_name6", 4);   // 1.2.3.4
    REQUIRE(esp_netif != NULL);
    char if_name[8] = { };
    const esp_err_t name_result = esp_netif_get_netif_impl_name(esp_netif, if_name);
    sock_addr.sin6_scope_id = (uint32_t)esp_netif_get_netif_impl_index(esp_netif);
    const int named_result = esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST);
    std::string named_host = buffer;
    esp_netif_destroy(esp_netif);
    REQUIRE(name_result == ESP_OK);
    CHECK(named_result == 0);
    CHECK(strcasecmp(("fe80::1%" + std::string(if_name)).c_str(), named_host.c_str()) == 0);

    sock_addr.sin6_scope_id = 200;  // no such interface
    CHECK(esp_getnameinfo((struct sockaddr *)&sock_addr, sizeof(sock_addr), buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) == 0);
    CHECK(strcasecmp("fe80::1%200", buffer) == 0);
}

static void test_getifaddr(int expected_nr_of_addrs)
{
    struct ifaddrs *addresses, *addr;
    int nr_of_addrs = 0;

    CHECK(esp_getifaddrs(&addresses) != -1);
    addr = addresses;

    while (addr) {
        ++nr_of_addrs;
        if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET) {   // look for IP4 addresses
            struct sockaddr_in *sock_addr = (struct sockaddr_in *) addr->ifa_addr;
            if (esp_getnameinfo((struct sockaddr *)sock_addr, sizeof(*sock_addr),
                                buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) != 0) {
                printf("esp_getnameinfo() failed\n");
            } else {
                printf("IPv4 address of interface \"%s\": %s\n", addr->ifa_name, buffer);
                // Match by IP only — do not assert lwIP names (stN). netif_num is
                // monotonic and not reused after destroy, so names shift if another
                // test (e.g. gethostname) created a netif earlier in this process.
                if (strcmp(buffer, "1.2.3.1") != 0 && strcmp(buffer, "1.2.3.2") != 0) {
                    FAIL("unexpected network interface address");
                }
            }
        }
        addr = addr->ifa_next;
    }
    // check that we got 1 address with exact content
    CHECK(nr_of_addrs == expected_nr_of_addrs);
    esp_freeifaddrs(addresses);
}

TEST_CASE("esp_getifaddrs() with 0, 1, and 2 addresses", "[sock_utils]")
{
    test_getifaddr(0);
    esp_netif_t *esp_netif = create_test_netif("station", 1);   // 1.2.3.1
    REQUIRE(esp_netif != NULL);
    test_getifaddr(1);
    esp_netif_t *esp_netif2 = create_test_netif("station2", 2); // 1.2.3.2
    REQUIRE(esp_netif2 != NULL);
    test_getifaddr(2);
    esp_netif_destroy(esp_netif);
    esp_netif_destroy(esp_netif2);
}

TEST_CASE("esp_getifaddrs() with IPv6 addresses", "[sock_utils]")
{
    esp_netif_t *esp_netif = create_test_netif("station_ip6", 3);   // 1.2.3.3
    REQUIRE(esp_netif != NULL);
    esp_ip6_addr_t link_local = { };
    esp_ip6_addr_t global = { };
    REQUIRE(esp_netif_str_to_ip6("fe80::1", &link_local) == ESP_OK);
    REQUIRE(esp_netif_str_to_ip6("2001:db8::1", &global) == ESP_OK);
    REQUIRE(esp_netif_add_ip6_address(esp_netif, link_local, true) == ESP_OK);
    REQUIRE(esp_netif_add_ip6_address(esp_netif, global, true) == ESP_OK);

    struct ifaddrs *addresses = NULL;
    const int result = esp_getifaddrs(&addresses);
    int nr_of_ip4 = 0, nr_of_link_local = 0, nr_of_global = 0, nr_of_other = 0;
    uint32_t link_local_scope = 0, global_scope = 0;
    for (struct ifaddrs *addr = addresses; addr; addr = addr->ifa_next) {
        if (addr->ifa_addr == NULL || addr->ifa_addr->sa_family != AF_INET6) {
            nr_of_ip4 += (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET);
            continue;
        }
        const struct sockaddr_in6 *sock_addr6 = (const struct sockaddr_in6 *) addr->ifa_addr;
        printf("IPv6 address of interface \"%s\": " IPV6STR "%%%u\n", addr->ifa_name,
               IPV62STR(*(const esp_ip6_addr_t *)&sock_addr6->sin6_addr), (unsigned)sock_addr6->sin6_scope_id);
        if (memcmp(&sock_addr6->sin6_addr, link_local.addr, sizeof(link_local.addr)) == 0) {
            ++nr_of_link_local;
            link_local_scope = sock_addr6->sin6_scope_id;
        } else if (memcmp(&sock_addr6->sin6_addr, global.addr, sizeof(global.addr)) == 0) {
            ++nr_of_global;
            global_scope = sock_addr6->sin6_scope_id;
        } else {
            ++nr_of_other;
        }
    }
    const uint32_t netif_index = (uint32_t)esp_netif_get_netif_impl_index(esp_netif);
    esp_freeifaddrs(addresses);
    esp_netif_destroy(esp_netif);

    CHECK(result != -1);
    // the IPv4 entry of the interface stays, and each IPv6 address gets its own entry
    CHECK(nr_of_ip4 == 1);
    CHECK(nr_of_link_local == 1);
    CHECK(nr_of_global == 1);
    CHECK(nr_of_other == 0);
    // a link-local address is scoped to its interface, like on Linux; a global one is not
    CHECK(link_local_scope == netif_index);
    CHECK(global_scope == 0);
}

static void test_pipe(int read_fd, int write_fd)
{
    CHECK(read_fd  >= 0);
    CHECK(write_fd >= 0);
    CHECK(write(write_fd, buffer, 1) > 0);
    CHECK(write(write_fd, buffer, 1) > 0);
    CHECK(read(read_fd, buffer, 1) == 1);
    CHECK(read(read_fd, buffer, 1) == 1);
}

TEST_CASE("poll.h declares the poll API", "[sock_utils]")
{
    struct pollfd pfd = { };
    pfd.fd = -1;
    pfd.events = POLLIN | POLLOUT;
    CHECK(pfd.revents == 0);
    // no descriptors and no timeout: returns at once
    CHECK(poll(&pfd, 0, 0) == 0);
}

TEST_CASE("socketpair()", "[sock_utils]")
{
    int fds[2];
    CHECK(esp_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    printf("socketpair created fds: %d, %d\n", fds[0], fds[1]);
    // check both directions
    test_pipe(fds[0], fds[1]);
    test_pipe(fds[1], fds[0]);
    close(fds[0]);
    close(fds[1]);
}

TEST_CASE("pipe()", "[sock_utils]")
{
    int fds[2];
    CHECK(esp_pipe(fds) == 0);
    printf("pipe created fds: %d, %d\n", fds[0], fds[1]);
    // check only one direction
    test_pipe(fds[0], fds[1]);
    close(fds[0]);
    close(fds[1]);
}

TEST_CASE("gai_strerror()", "[sock_utils]")
{
    const char *str_error = esp_gai_strerror(EAI_BADFLAGS);
    CHECK(str_error != NULL);
}

TEST_CASE("gethostname()", "[sock_utils]")
{
    const char *test_netif_name = "station";
    char hostname[32];
    int ret;

    // expect failure
    ret = gethostname(hostname, strlen(CONFIG_LWIP_LOCAL_HOSTNAME) - 1);
    CHECK(ret == -1);

    // happy flow with the default name
    ret = gethostname(hostname, sizeof(hostname));
    CHECK(ret == 0);
    CHECK(strcmp(hostname, CONFIG_LWIP_LOCAL_HOSTNAME) == 0);

    // happy flow with the netif name
    esp_netif_t *esp_netif = create_test_netif(test_netif_name, 1);
    REQUIRE(esp_netif != NULL);
    CHECK(esp_netif_set_hostname(esp_netif, test_netif_name) == ESP_OK);
    ret = gethostname(hostname, sizeof(hostname));
    CHECK(ret == 0);
    CHECK(strcmp(hostname, test_netif_name) == 0);
    esp_netif_destroy(esp_netif);
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    Catch::Session session;

    int failures = session.run();
    if (failures > 0) {
        printf("TEST FAILED! number of failures=%d\n", failures);
        exit(1);
    } else {
        printf("Test passed!\n");
        exit(0);
    }
}
