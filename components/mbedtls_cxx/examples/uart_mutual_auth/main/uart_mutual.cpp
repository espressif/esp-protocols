/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <thread>
#include <sys/socket.h>
#include <unistd.h>
#include <esp_check.h>
#include "esp_log.h"
#include "mbedtls_wrap.hpp"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_netif.h"

static auto const *TAG = "uart_mutual_tls";

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

/**
 * Using DTLS the below is set to true.
 * In that case, we need to receive the entire datagram, not a fragment
 * This defines a very simple datagram protocol over UART:
 *  | HEADER (2bytes) | PAYLOAD ...   |
 *  | dgram_len       | dgram_payload |
 *
 * If `use_dgrams` is set to false, we perform TLS on UART stream.
 * The UART driver is already a stream-like API (using ringbufer), so we simple read and write to UART
 */
static const bool use_dgrams = false;

class SecureLink: public Tls {
public:
    explicit SecureLink(uart_port_t port, int tx, int rx) : Tls(), uart(port, tx, rx) {}
    ~SecureLink() = default;
    int send(const unsigned char *buf, size_t len) override
    {
        if (use_dgrams) {
            // sends a separate dgram header
            uint16_t header = len;
            uart_write_bytes(uart.port_, &header, 2);
        }
        return uart_write_bytes(uart.port_, buf, len);
    }

    int recv(unsigned char *buf, size_t len) override
    {
        // stream read
        return uart.recv(buf, len, 0);
    }

    int recv_tout(unsigned char *buf, size_t len, int timeout) override
    {
        // dgram read
        return uart.recv_dgram(buf, len, timeout);
    }

    bool open(bool server_not_client)
    {
        if (uart.init() != ESP_OK) {
            return false;
        }
        while (!uart.debounce(server_not_client)) {
            printf("debouncing...\n");
            usleep(10000);
        }
        TlsConfig config{};
        config.is_dtls = use_dgrams;
        config.timeout = 10000;
        if (server_not_client) {
            const unsigned char client_id[] = "Client1";
            config.client_id = std::make_pair(client_id, sizeof(client_id));
        }
        if (!init(is_server{server_not_client}, do_verify{false}, &config)) {
            return false;
        }

        return handshake() == 0;
    }

private:
    /**
     * RAII wrapper of UART
     */
    struct uart_info {
        uart_port_t port_;
        QueueHandle_t queue_{};
        int tx_, rx_;

        // used for datagrams
        bool header_{true};
        int in_payload_{0};
        int payload_len_{0};
        uint8_t payload_[1600] {};

        explicit uart_info(uart_port_t port, int tx, int rx): port_(port), tx_(tx), rx_(rx)
        {
        }
        esp_err_t init()
        {
            uart_config_t uart_config = {};
            uart_config.baud_rate = 115200;
            uart_config.data_bits = UART_DATA_8_BITS;
            uart_config.parity    = UART_PARITY_DISABLE;
            uart_config.stop_bits = UART_STOP_BITS_1;
            uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
            uart_config.source_clk = UART_SCLK_DEFAULT;
            ESP_RETURN_ON_ERROR(uart_driver_install(port_, 1024, 0, 1, &queue_, 0), TAG, "Failed to install UART");
            ESP_RETURN_ON_ERROR(uart_param_config(port_, &uart_config), TAG, "Failed to set params");
            ESP_RETURN_ON_ERROR(uart_set_pin(port_, tx_, rx_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "Failed to set UART pins");
            ESP_RETURN_ON_ERROR(uart_set_rx_timeout(port_, 10), TAG, "Failed to set UART Rx timeout");
            return ESP_OK;
        }
        ~uart_info()
        {
            uart_driver_delete(port_);
        }
        bool debounce(bool server)
        {
            uint8_t data = 0;
            if (server) {
                while (uart_read_bytes(port_, &data, 1, 0) != 0) {
                    if (data == 0x55) {
                        uart_write_bytes(port_, &data, 1);
                        return true;
                    }
                }
                return false;
            }
            data = 0x55;
            uart_write_bytes(port_, &data, 1);
            data = 0;
            uart_read_bytes(port_, &data, 1, pdMS_TO_TICKS(1000));
            if (data != 0x55) {
                uart_flush_input(port_);
                return false;
            }
            return true;
        }

        int recv(unsigned char *buf, size_t size, int timeout) // this is for stream transport
        {
            int len = uart_read_bytes(port_, buf, size, pdMS_TO_TICKS(timeout));
            if (len == 0) {
                return MBEDTLS_ERR_SSL_WANT_READ;
            }
            return len;
        }

        int recv_dgram(unsigned char *buf, size_t size, int timeout) // this is for datagrams
        {
            uart_event_t event = {};
            size_t length;
            uart_get_buffered_data_len(port_, &length);
            if (length == 0) {
                xQueueReceive(queue_, &event, pdMS_TO_TICKS(timeout));
            }
            uart_get_buffered_data_len(port_, &length);
            if (length == 0) {
                return MBEDTLS_ERR_SSL_WANT_READ;
            }
            if (header_) {
                if (length >= 2) {
                    uart_read_bytes(port_, &payload_len_, 2, 0);
                    header_ = false;
                    length -= 2;
                }
            }
            if (!header_ && length > 0) {
                int to_read = payload_len_ - in_payload_;
                int l = uart_read_bytes(port_, &payload_[in_payload_], to_read, 0);
                in_payload_ += l;
                if (payload_len_ == in_payload_) {
                    header_ = true;
                    memcpy(buf, payload_, payload_len_);
                    in_payload_ = 0;
                    return payload_len_;
                }
            }
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
    } uart;
};

static void tls_client()
{
    const unsigned char message[] = "Hello\n";
    unsigned char reply[128];
    SecureLink client(UART_NUM_2, 4, 5);
    if (!client.open(false)) {
        ESP_LOGE(TAG, "Failed to CONNECT! %d", errno);
        return;
    }
    ESP_LOGI(TAG, "client opened...");
    if (client.write(message, sizeof(message)) < 0) {
        ESP_LOGE(TAG, "Failed to write!");
        return;
    }

    int len;
    while ((len = client.read(reply, sizeof(reply))) == MBEDTLS_ERR_SSL_WANT_READ) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to read!");
        return;
    }
    ESP_LOGI(TAG, "Successfully received: %.*s", len, reply);
}

static void tls_server()
{
    unsigned char message[128];
    SecureLink server(UART_NUM_1, 25, 26);
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
    int len;
    while ((len = server.read(message, sizeof(message))) == MBEDTLS_ERR_SSL_WANT_READ) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to read! %x", -len);
        return;
    }
    ESP_LOGI(TAG, "Received from client: %.*s", len, message);
    if (server.write(message, len) < 0) {
        ESP_LOGE(TAG, "Failed to write!");
        return;
    }
    ESP_LOGI(TAG, "Written back");
    vTaskDelay(pdMS_TO_TICKS(500));
}


extern "C" void app_main()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    std::thread t2(tls_server);
    std::thread t1(tls_client);
    t1.join();
    t2.join();
}
