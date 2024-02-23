/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "esp_log.h"
#include "mbedtls_wrap.hpp"

using namespace idf::mbedtls_cxx;

namespace {
constexpr auto *TAG = "simple_tls_client";
}

class TlsSocketClient: public Tls {
public:
    TlsSocketClient() = default;
    ~TlsSocketClient() override
    {
        if (sock >= 0) {
            ::close(sock);
        }
    }
    int send(const unsigned char *buf, size_t len) override
    {
        return ::send(sock, buf, len, 0);
    }
    int recv(unsigned char *buf, size_t len) override
    {
        return ::recv(sock, buf, len, 0);
    }
    bool connect(const char *host, int port)
    {
        addr_info addr(host, AF_INET, SOCK_STREAM);
        if (!addr) {
            ESP_LOGE(TAG, "Failed to resolve host");
            return false;
        }
        sock = addr.get_sock();
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create socket");
            return false;
        }

        if (::connect(sock, addr.get_addr(port), sizeof(struct sockaddr)) < 0) {
            ESP_LOGE(TAG, "Failed to connect %d", errno);
            return false;
        }

        if (!init(is_server{false}, do_verify{false})) {
            return false;
        }
        return handshake() == 0;
    }

private:
    int sock{-1};
    /**
     * RAII wrapper of the address_info
     */
    struct addr_info {
        struct addrinfo *ai {
            nullptr
        };
        ~addr_info()
        {
            freeaddrinfo(ai);
        }
        explicit addr_info(const char *host, int family, int type)
        {
            struct addrinfo hints {};
            hints.ai_family = family;
            hints.ai_socktype = type;
            if (getaddrinfo(host, nullptr, &hints, &ai) < 0) {
                freeaddrinfo(ai);
                ai = nullptr;
            }
        }
        explicit operator bool() const
        {
            return ai != nullptr;
        }
        struct sockaddr *get_addr(uint16_t port) const {
            auto *p = (struct sockaddr_in *)ai->ai_addr;
            p->sin_port = htons(port);
            return (struct sockaddr *)p;
        }
        int get_sock() const
        {
            return socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        }
    };
};

namespace {

void tls_client()
{
    const unsigned char message[] = "Hello\n";
    unsigned char reply[128];
    TlsSocketClient client;
    if (!client.connect("tcpbin.com", 4243)) {
        ESP_LOGE(TAG, "Failed to connect! %d", errno);
        return;
    }
    if (client.write(message, sizeof(message)) < 0) {
        ESP_LOGE(TAG, "Failed to write!");
        return;
    }
    int len = client.read(reply, sizeof(reply));
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to read!");
        return;
    }
    ESP_LOGI(TAG, "Successfully received: %.*s", len, reply);
}

} // namespace

#if CONFIG_IDF_TARGET_LINUX
/**
 * Linux target: We're already connected, just run the client
 */
int main()
{
    tls_client();
    return 0;
}
#else
/**
 * ESP32 chipsets:  Need to initialize system components
 *                  and connect to network
 */

#include "nvs_flash.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "esp_netif.h"

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    tls_client();
}
#endif
