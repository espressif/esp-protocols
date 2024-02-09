/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <thread>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "esp_log.h"
#include "mbedtls_wrap.hpp"

static auto const *TAG = "simple_udp_example";

const unsigned char servercert[] = "-----BEGIN CERTIFICATE-----\n"
                                   "MIIDKzCCAhOgAwIBAgIUBxM3WJf2bP12kAfqhmhhjZWv0ukwDQYJKoZIhvcNAQEL\n"
                                   "BQAwJTEjMCEGA1UEAwwaRVNQMzIgSFRUUFMgc2VydmVyIGV4YW1wbGUwHhcNMTgx\n"
                                   "MDE3MTEzMjU3WhcNMjgxMDE0MTEzMjU3WjAlMSMwIQYDVQQDDBpFU1AzMiBIVFRQ\n"
                                   "UyBzZXJ2ZXIgZXhhbXBsZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
                                   "ALBint6nP77RCQcmKgwPtTsGK0uClxg+LwKJ3WXuye3oqnnjqJCwMEneXzGdG09T\n"
                                   "sA0SyNPwrEgebLCH80an3gWU4pHDdqGHfJQa2jBL290e/5L5MB+6PTs2NKcojK/k\n"
                                   "qcZkn58MWXhDW1NpAnJtjVniK2Ksvr/YIYSbyD+JiEs0MGxEx+kOl9d7hRHJaIzd\n"
                                   "GF/vO2pl295v1qXekAlkgNMtYIVAjUy9CMpqaQBCQRL+BmPSJRkXBsYk8GPnieS4\n"
                                   "sUsp53DsNvCCtWDT6fd9D1v+BB6nDk/FCPKhtjYOwOAZlX4wWNSZpRNr5dfrxKsb\n"
                                   "jAn4PCuR2akdF4G8WLUeDWECAwEAAaNTMFEwHQYDVR0OBBYEFMnmdJKOEepXrHI/\n"
                                   "ivM6mVqJgAX8MB8GA1UdIwQYMBaAFMnmdJKOEepXrHI/ivM6mVqJgAX8MA8GA1Ud\n"
                                   "EwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBADiXIGEkSsN0SLSfCF1VNWO3\n"
                                   "emBurfOcDq4EGEaxRKAU0814VEmU87btIDx80+z5Dbf+GGHCPrY7odIkxGNn0DJY\n"
                                   "W1WcF+DOcbiWoUN6DTkAML0SMnp8aGj9ffx3x+qoggT+vGdWVVA4pgwqZT7Ybntx\n"
                                   "bkzcNFW0sqmCv4IN1t4w6L0A87ZwsNwVpre/j6uyBw7s8YoJHDLRFT6g7qgn0tcN\n"
                                   "ZufhNISvgWCVJQy/SZjNBHSpnIdCUSJAeTY2mkM4sGxY0Widk8LnjydxZUSxC3Nl\n"
                                   "hb6pnMh3jRq4h0+5CZielA4/a+TdrNPv/qok67ot/XJdY3qHCCd8O2b14OVq9jo=\n"
                                   "-----END CERTIFICATE-----";
const unsigned char prvtkey[] = "-----BEGIN PRIVATE KEY-----\n"
                                "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwYp7epz++0QkH\n"
                                "JioMD7U7BitLgpcYPi8Cid1l7snt6Kp546iQsDBJ3l8xnRtPU7ANEsjT8KxIHmyw\n"
                                "h/NGp94FlOKRw3ahh3yUGtowS9vdHv+S+TAfuj07NjSnKIyv5KnGZJ+fDFl4Q1tT\n"
                                "aQJybY1Z4itirL6/2CGEm8g/iYhLNDBsRMfpDpfXe4URyWiM3Rhf7ztqZdveb9al\n"
                                "3pAJZIDTLWCFQI1MvQjKamkAQkES/gZj0iUZFwbGJPBj54nkuLFLKedw7DbwgrVg\n"
                                "0+n3fQ9b/gQepw5PxQjyobY2DsDgGZV+MFjUmaUTa+XX68SrG4wJ+DwrkdmpHReB\n"
                                "vFi1Hg1hAgMBAAECggEAaTCnZkl/7qBjLexIryC/CBBJyaJ70W1kQ7NMYfniWwui\n"
                                "f0aRxJgOdD81rjTvkINsPp+xPRQO6oOadjzdjImYEuQTqrJTEUnntbu924eh+2D9\n"
                                "Mf2CAanj0mglRnscS9mmljZ0KzoGMX6Z/EhnuS40WiJTlWlH6MlQU/FDnwC6U34y\n"
                                "JKy6/jGryfsx+kGU/NRvKSru6JYJWt5v7sOrymHWD62IT59h3blOiP8GMtYKeQlX\n"
                                "49om9Mo1VTIFASY3lrxmexbY+6FG8YO+tfIe0tTAiGrkb9Pz6tYbaj9FjEWOv4Vc\n"
                                "+3VMBUVdGJjgqvE8fx+/+mHo4Rg69BUPfPSrpEg7sQKBgQDlL85G04VZgrNZgOx6\n"
                                "pTlCCl/NkfNb1OYa0BELqWINoWaWQHnm6lX8YjrUjwRpBF5s7mFhguFjUjp/NW6D\n"
                                "0EEg5BmO0ePJ3dLKSeOA7gMo7y7kAcD/YGToqAaGljkBI+IAWK5Su5yldrECTQKG\n"
                                "YnMKyQ1MWUfCYEwHtPvFvE5aPwKBgQDFBWXekpxHIvt/B41Cl/TftAzE7/f58JjV\n"
                                "MFo/JCh9TDcH6N5TMTRS1/iQrv5M6kJSSrHnq8pqDXOwfHLwxetpk9tr937VRzoL\n"
                                "CuG1Ar7c1AO6ujNnAEmUVC2DppL/ck5mRPWK/kgLwZSaNcZf8sydRgphsW1ogJin\n"
                                "7g0nGbFwXwKBgQCPoZY07Pr1TeP4g8OwWTu5F6dSvdU2CAbtZthH5q98u1n/cAj1\n"
                                "noak1Srpa3foGMTUn9CHu+5kwHPIpUPNeAZZBpq91uxa5pnkDMp3UrLIRJ2uZyr8\n"
                                "4PxcknEEh8DR5hsM/IbDcrCJQglM19ZtQeW3LKkY4BsIxjDf45ymH407IQKBgE/g\n"
                                "Ul6cPfOxQRlNLH4VMVgInSyyxWx1mODFy7DRrgCuh5kTVh+QUVBM8x9lcwAn8V9/\n"
                                "nQT55wR8E603pznqY/jX0xvAqZE6YVPcw4kpZcwNwL1RhEl8GliikBlRzUL3SsW3\n"
                                "q30AfqEViHPE3XpE66PPo6Hb1ymJCVr77iUuC3wtAoGBAIBrOGunv1qZMfqmwAY2\n"
                                "lxlzRgxgSiaev0lTNxDzZkmU/u3dgdTwJ5DDANqPwJc6b8SGYTp9rQ0mbgVHnhIB\n"
                                "jcJQBQkTfq6Z0H6OoTVi7dPs3ibQJFrtkoyvYAbyk36quBmNRjVh6rc8468bhXYr\n"
                                "v/t+MeGJP/0Zw8v/X2CFll96\n"
                                "-----END PRIVATE KEY-----";


class SecureLink: public Tls {
public:
    explicit SecureLink() : Tls(), addr("localhost", 3333, AF_INET, SOCK_DGRAM) {}
    ~SecureLink() override
    {
        if (sock >= 0) {
            ::close(sock);
        }
    }
    int send(const unsigned char *buf, size_t len) override
    {
        return sendto(sock, buf, len, 0, addr, ai_size);
    }
    int recv(unsigned char *buf, size_t len) override
    {
        socklen_t socklen = sizeof(sockaddr);
        return recvfrom(sock, buf, len, 0, addr, &socklen);
    }
    int recv_tout(unsigned char *buf, size_t len, int timeout) override
    {
        struct timeval tv {
            timeout / 1000, (timeout % 1000 ) * 1000
        };
        fd_set read_fds;
        FD_ZERO( &read_fds );
        FD_SET( sock, &read_fds );

        int ret = select(sock + 1, &read_fds, nullptr, nullptr, timeout == 0 ? nullptr : &tv);
        if (ret == 0) {
            return MBEDTLS_ERR_SSL_TIMEOUT;
        }
        if (ret < 0) {
            if (errno == EINTR) {
                return MBEDTLS_ERR_SSL_WANT_READ;
            }
            return ret;
        }
        return recv(buf, len);
    }
    bool open(bool server_not_client)
    {
        if (!addr) {
            ESP_LOGE(TAG, "Failed to resolve endpoint");
            return false;
        }
        sock = addr.get_sock();
        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create socket");
            return false;
        }
        if (server_not_client) {
            int err = bind(sock, addr, ai_size);
            if (err < 0) {
                ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
                return false;
            }
        }
        if (!init(is_server{server_not_client}, do_verify{false})) {
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
        struct addrinfo *ai = nullptr;
        explicit addr_info(const char *host, int port, int family, int type)
        {
            struct addrinfo hints {};
            hints.ai_family = family;
            hints.ai_socktype = type;
            if (getaddrinfo(host, nullptr, &hints, &ai) < 0) {
                freeaddrinfo(ai);
                ai = nullptr;
            }
            auto *p = (struct sockaddr_in *)ai->ai_addr;
            p->sin_port = htons(port);
        }
        ~addr_info()
        {
            freeaddrinfo(ai);
        }
        explicit operator bool() const
        {
            return ai != nullptr;
        }
        operator sockaddr *() const
        {
            auto *p = (struct sockaddr_in *)ai->ai_addr;
            return (struct sockaddr *)p;
        }

        int get_sock() const
        {
            return socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        }
    } addr;
    const int ai_size{sizeof(struct sockaddr_in)};
};

static void tls_client()
{
    const unsigned char message[] = "Hello\n";
    unsigned char reply[128];
    SecureLink client;
    if (!client.open(false)) {
        ESP_LOGE(TAG, "Failed to CONNECT! %d", errno);
        return;
    }
    ESP_LOGI(TAG, "client opened...");
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

static void tls_server()
{
    unsigned char message[128];
    SecureLink server;
    const_buf cert{servercert, sizeof(servercert)};
    const_buf key{prvtkey, sizeof(prvtkey)};
    if (!server.set_own_cert(cert, key)) {
        ESP_LOGE(TAG, "Failed to set own cert");
        return;
    }
    ESP_LOGI(TAG, "openning...");
    if (!server.open(true)) {
        ESP_LOGE(TAG, "Failed to OPEN! %d", errno);
        return;
    }
    int len = server.read(message, sizeof(message));
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to read!");
        return;
    }
    ESP_LOGI(TAG, "Received from client: %.*s", len, message);
    if (server.write(message, len) < 0) {
        ESP_LOGE(TAG, "Failed to write!");
        return;
    }
    ESP_LOGI(TAG, "Written back");
}

static void udp_auth()
{
    std::thread t2(tls_server);
//    usleep(1000);
    std::thread t1(tls_client);
    t1.join();
    t2.join();
}

#if CONFIG_IDF_TARGET_LINUX
/**
 * Linux target: We're already connected, just run the client
 */
int main()
{
    udp_auth();
    return 0;
}
#else
/**
 * ESP32 chipsets:  Need to initialize system components
 *                  and connect to network
 */

#include "esp_event.h"
#include "esp_netif.h"

extern "C" void app_main()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    udp_auth();
}
#endif
