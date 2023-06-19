/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_transport.h"
#include "mbedtls_wrap.hpp"

static const char *TAG = "tls_transport";

class TlsTransport: public Tls {
public:
    explicit TlsTransport(esp_transport_handle_t parent) : Tls(), transport_(parent) {}
    int send(const unsigned char *buf, size_t len) override;
    int recv(unsigned char *buf, size_t len) override;
    static bool set_func(esp_transport_handle_t tls_transport);

private:
    esp_transport_handle_t transport_{};
    int connect(const char *host, int port, int timeout_ms);

    struct priv {
        static int connect(esp_transport_handle_t t, const char *host, int port, int timeout_ms);
        static int read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms);
        static int write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms);
        static int close(esp_transport_handle_t t);
        static int poll_read(esp_transport_handle_t t, int timeout_ms);
        static int poll_write(esp_transport_handle_t t, int timeout_ms);
        static int destroy(esp_transport_handle_t t);
    };
};

esp_transport_handle_t esp_transport_tls_init(esp_transport_handle_t parent)
{
    esp_transport_handle_t ssl = esp_transport_init();
    auto *tls = new TlsTransport(parent);
    esp_transport_set_context_data(ssl, tls);
    TlsTransport::set_func(ssl);
    return ssl;
}

int TlsTransport::send(const unsigned char *buf, size_t len)
{
    return esp_transport_write(transport_, reinterpret_cast<const char *>(buf), len, 0);
}

int TlsTransport::recv(unsigned char *buf, size_t len)
{
    int ret = esp_transport_read(transport_, reinterpret_cast<char *>(buf), len, 0);

    if (ret == ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return ret == ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN ? 0 : ret;
}

bool TlsTransport::set_func(esp_transport_handle_t tls_transport)
{
    return esp_transport_set_func(tls_transport, TlsTransport::priv::connect, TlsTransport::priv::read, TlsTransport::priv::write, TlsTransport::priv::close, TlsTransport::priv::poll_read, TlsTransport::priv::poll_write, TlsTransport::priv::destroy) == ESP_OK;
}

int TlsTransport::connect(const char *host, int port, int timeout_ms)
{
    return esp_transport_connect(transport_, host, port, timeout_ms);
}

int TlsTransport::priv::connect(esp_transport_handle_t t, const char *host, int port, int timeout_ms)
{
    ESP_LOGI("tag", "SSL connect!");
    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    tls->init(false, false);

    ESP_LOGI("tag", "TCP connect!");
    auto ret = tls->connect(host, port, timeout_ms);
    if (ret < 0) {
        ESP_LOGI("tag", "TCP connect fail!");
        return ret;
    }
    return tls->handshake();
}

int TlsTransport::priv::read(esp_transport_handle_t t, char *buffer, int len, int timeout_ms)
{
    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    if (tls->get_available_bytes() <= 0) {
        int poll = esp_transport_poll_read(t, timeout_ms);
        if (poll == -1) {
            return ERR_TCP_TRANSPORT_CONNECTION_FAILED;
        }
        if (poll == 0) {
            return ERR_TCP_TRANSPORT_CONNECTION_TIMEOUT;
        }
    }
    return tls->read(reinterpret_cast<unsigned char *>(buffer), len);
}

int TlsTransport::priv::write(esp_transport_handle_t t, const char *buffer, int len, int timeout_ms)
{
    int poll;
    if ((poll = esp_transport_poll_write(t, timeout_ms)) <= 0) {
        ESP_LOGW(TAG, "Poll timeout or error timeout_ms=%d", timeout_ms);
        return poll;
    }

    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    return tls->write(reinterpret_cast<const unsigned char *>(buffer), len);
}

int TlsTransport::priv::close(esp_transport_handle_t t)
{
    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    return esp_transport_close(tls->transport_);
}

int TlsTransport::priv::poll_read(esp_transport_handle_t t, int timeout_ms)
{
    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    return esp_transport_poll_read(tls->transport_, timeout_ms);
}

int TlsTransport::priv::poll_write(esp_transport_handle_t t, int timeout_ms)
{
    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    return esp_transport_poll_write(tls->transport_, timeout_ms);
}

int TlsTransport::priv::destroy(esp_transport_handle_t t)
{
    auto tls = static_cast<TlsTransport *>(esp_transport_get_context_data(t));
    return esp_transport_destroy(tls->transport_);
}
