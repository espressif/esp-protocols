/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "mosq_broker.h"

namespace {

bool mqtt_encode_length(size_t len, std::vector<uint8_t> &out)
{
    if (len > 268435455) {
        return false;
    }
    do {
        uint8_t b = len % 128;
        len /= 128;
        if (len > 0) {
            b |= 0x80;
        }
        out.push_back(b);
    } while (len > 0);
    return true;
}

void mqtt_append_string(std::vector<uint8_t> &buf, const char *s)
{
    size_t n = strlen(s);
    buf.push_back((uint8_t)((n >> 8) & 0xff));
    buf.push_back((uint8_t)(n & 0xff));
    buf.insert(buf.end(), s, s + n);
}

int mqtt_connect_tcp(const char *host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, (sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool mqtt_send_all(int fd, const uint8_t *data, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, data + off, len - off, 0);
        if (n <= 0) {
            return false;
        }
        off += (size_t)n;
    }
    return true;
}

bool mqtt_recv_exact(int fd, uint8_t *data, size_t len, int timeout_ms)
{
    size_t off = 0;
    while (off < len) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        timeval tv = {};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int sel = select(fd + 1, &rfds, nullptr, nullptr, &tv);
        if (sel <= 0) {
            return false;
        }
        ssize_t n = recv(fd, data + off, len - off, 0);
        if (n <= 0) {
            return false;
        }
        off += (size_t)n;
    }
    return true;
}

bool mqtt_expect_packet(int fd, uint8_t type_mask, int timeout_ms)
{
    uint8_t hdr;
    if (!mqtt_recv_exact(fd, &hdr, 1, timeout_ms)) {
        return false;
    }
    if ((hdr & 0xf0) != type_mask) {
        return false;
    }
    size_t remaining = 0;
    size_t mult = 1;
    for (int i = 0; i < 4; i++) {
        uint8_t b;
        if (!mqtt_recv_exact(fd, &b, 1, timeout_ms)) {
            return false;
        }
        remaining += (size_t)(b & 0x7f) * mult;
        if ((b & 0x80) == 0) {
            break;
        }
        mult *= 128;
    }
    if (remaining == 0) {
        return true;
    }
    std::vector<uint8_t> payload(remaining);
    return mqtt_recv_exact(fd, payload.data(), remaining, timeout_ms);
}

/* MQTT 3.1.1 CONNECT with optional Last-Will. */
bool mqtt_connect_with_will(int fd, const char *client_id,
                            const char *will_topic, const char *will_payload, bool will_retain)
{
    std::vector<uint8_t> vh;
    mqtt_append_string(vh, "MQTT");
    vh.push_back(0x04); /* protocol level 3.1.1 */
    uint8_t flags = 0x02; /* clean session */
    if (will_topic && will_payload) {
        flags |= 0x04; /* will */
        if (will_retain) {
            flags |= 0x20;
        }
    }
    vh.push_back(flags);
    vh.push_back(0x00);
    vh.push_back(0x3c); /* keepalive 60s */

    std::vector<uint8_t> payload;
    mqtt_append_string(payload, client_id);
    if (will_topic && will_payload) {
        mqtt_append_string(payload, will_topic);
        mqtt_append_string(payload, will_payload);
    }

    std::vector<uint8_t> pkt;
    pkt.push_back(0x10);
    if (!mqtt_encode_length(vh.size() + payload.size(), pkt)) {
        return false;
    }
    pkt.insert(pkt.end(), vh.begin(), vh.end());
    pkt.insert(pkt.end(), payload.begin(), payload.end());

    if (!mqtt_send_all(fd, pkt.data(), pkt.size())) {
        return false;
    }
    return mqtt_expect_packet(fd, 0x20, 2000); /* CONNACK */
}

bool mqtt_subscribe(int fd, const char *topic)
{
    std::vector<uint8_t> body;
    body.push_back(0x00);
    body.push_back(0x01); /* packet id */
    mqtt_append_string(body, topic);
    body.push_back(0x00); /* QoS 0 */

    std::vector<uint8_t> pkt;
    pkt.push_back(0x82);
    if (!mqtt_encode_length(body.size(), pkt)) {
        return false;
    }
    pkt.insert(pkt.end(), body.begin(), body.end());
    if (!mqtt_send_all(fd, pkt.data(), pkt.size())) {
        return false;
    }
    return mqtt_expect_packet(fd, 0x90, 2000); /* SUBACK */
}

void wait_broker_ready(const char *host, int port, int timeout_ms)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        int fd = mqtt_connect_tcp(host, port);
        if (fd >= 0) {
            close(fd);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    FAIL("broker did not accept connections in time");
}

} // namespace

TEST_CASE("Start and stop mosquitto broker", "[mosquitto]")
{
    struct mosq_broker_config config = {};
    config.host = "127.0.0.1";
    config.port = 18833;

    int broker_rc = -1;
    std::thread broker_thread([&]() {
        broker_rc = mosq_broker_run(&config);
    });

    wait_broker_ready(config.host, config.port, 3000);
    mosq_broker_stop();
    broker_thread.join();

    CHECK(broker_rc == 0);
}

TEST_CASE("Restart mosquitto broker after stop", "[mosquitto]")
{
    struct mosq_broker_config config = {};
    config.host = "127.0.0.1";
    config.port = 18834;

    auto run_broker = [&]() {
        int broker_rc = -1;
        std::thread broker_thread([&]() {
            broker_rc = mosq_broker_run(&config);
        });

        wait_broker_ready(config.host, config.port, 3000);
        mosq_broker_stop();
        broker_thread.join();

        return broker_rc;
    };

    CHECK(run_broker() == 0);
    CHECK(run_broker() == 0);
}

/*
 * Regression: stopping the broker while a Will publisher and a matching
 * subscriber are still connected used to NULL-deref in mux_poll__add()
 * because mux was freed before post-loop Will delivery.
 */
TEST_CASE("Stop broker with connected Will client and subscriber", "[mosquitto]")
{
    struct mosq_broker_config config = {};
    config.host = "127.0.0.1";
    config.port = 18835;

    int broker_rc = -1;
    std::thread broker_thread([&]() {
        broker_rc = mosq_broker_run(&config);
    });

    wait_broker_ready(config.host, config.port, 3000);

    int sub_fd = mqtt_connect_tcp(config.host, config.port);
    REQUIRE(sub_fd >= 0);
    REQUIRE(mqtt_connect_with_will(sub_fd, "sub-client", nullptr, nullptr, false));
    REQUIRE(mqtt_subscribe(sub_fd, "test/#"));

    int pub_fd = mqtt_connect_tcp(config.host, config.port);
    REQUIRE(pub_fd >= 0);
    REQUIRE(mqtt_connect_with_will(pub_fd, "will-client", "test/will", "bye", true));

    /* Let the broker register both sessions before teardown. */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    mosq_broker_stop();
    broker_thread.join();

    close(sub_fd);
    close(pub_fd);

    CHECK(broker_rc == 0);
}

extern "C" void app_main(void)
{
    int result = Catch::Session().run();
    if (result != 0) {
        printf("Test failed with result %d.\n", result);
    } else {
        printf("All tests passed successfully.\n");
    }
    std::exit(result);
}
