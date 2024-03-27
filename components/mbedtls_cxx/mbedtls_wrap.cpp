/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <mbedtls/timing.h>
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h"
#include "mbedtls_wrap.hpp"

using namespace idf::mbedtls_cxx;

bool Tls::init(is_server server, do_verify verify, TlsConfig *config)
{
    const char pers[] = "mbedtls_wrapper";
    is_server_ = server == is_server{true};
    is_dtls_ = config ? config->is_dtls : false;
    uint32_t timeout = config ? config->timeout : 0;
    mbedtls_entropy_init(&entropy_);
    mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_, (const unsigned char *)pers, sizeof(pers));
    int endpoint = server == is_server{true} ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    int transport = is_dtls_ ? MBEDTLS_SSL_TRANSPORT_DATAGRAM : MBEDTLS_SSL_TRANSPORT_STREAM;
    int ret = mbedtls_ssl_config_defaults(&conf_, endpoint, transport, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret) {
        print_error("mbedtls_ssl_config_defaults", ret);
        return false;
    }
    mbedtls_ssl_conf_rng(&conf_, mbedtls_ctr_drbg_random, &ctr_drbg_);
    if (timeout) {
        mbedtls_ssl_conf_read_timeout(&conf_, timeout);
    }
    mbedtls_ssl_conf_authmode(&conf_, verify == do_verify{true} ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
    ret = mbedtls_ssl_conf_own_cert(&conf_, &public_cert_, &pk_key_);
    if (ret) {
        print_error("mbedtls_ssl_conf_own_cert", ret);
        return false;
    }
    if (verify == do_verify{true}) {
        mbedtls_ssl_conf_ca_chain(&conf_, &ca_cert_, nullptr);
    }

#if CONFIG_MBEDTLS_SSL_PROTO_DTLS
    if (is_server_ && is_dtls_) {
        if (!init_dtls_cookies()) {
            return false;
        }
    }
#endif // MBEDTLS_SSL_PROTO_DTLS

    ret = mbedtls_ssl_setup(&ssl_, &conf_);
    if (ret) {
        print_error("mbedtls_ssl_setup", ret);
        return false;
    }

    if (timeout) {
        mbedtls_ssl_set_timer_cb(&ssl_, &timer_, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
    }

#if CONFIG_MBEDTLS_SSL_PROTO_DTLS
    if (is_server_ && is_dtls_ && config && config->client_id != const_buf {}) {
        client_id_ = config->client_id;
        if (!set_client_id()) {
            return false;
        }
    }
#endif // MBEDTLS_SSL_PROTO_DTLS

    return true;
}

bool Tls::deinit()
{
    ::mbedtls_ssl_config_free(&conf_);
    ::mbedtls_ssl_free(&ssl_);
    ::mbedtls_pk_free(&pk_key_);
    ::mbedtls_x509_crt_free(&public_cert_);
    ::mbedtls_x509_crt_free(&ca_cert_);
    return true;
}

void Tls::print_error(const char *function, int error_code)
{
    static char error_buf[100];
    mbedtls_strerror(error_code, error_buf, sizeof(error_buf));

    printf("%s() returned -0x%04X\n", function, -error_code);
    printf("-0x%04X: %s\n", -error_code, error_buf);
}

int Tls::handshake()
{
    int ret = 0;
    mbedtls_ssl_set_bio(&ssl_, this, bio_write, bio_read, is_dtls_ ? bio_read_tout : nullptr);

    while ( ( ret = mbedtls_ssl_handshake( &ssl_ ) ) != 0 ) {
        if ( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE ) {
#if CONFIG_MBEDTLS_SSL_PROTO_DTLS
            if (is_server_ && is_dtls_ && ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
                // hello verification requested -> restart the session with this client_id
                if (!set_client_id()) {
                    return -1;
                }
                continue;
            }
#endif // MBEDTLS_SSL_PROTO_DTLS
            print_error( "mbedtls_ssl_handshake returned", ret );
            return -1;
        }
        delay();
    }
    return ret;
}

int Tls::bio_write(void *ctx, const unsigned char *buf, size_t len)
{
    auto s = static_cast<Tls *>(ctx);
    return s->send(buf, len);
}

int Tls::bio_read(void *ctx, unsigned char *buf, size_t len)
{
    auto s = static_cast<Tls *>(ctx);
    return s->recv(buf, len);
}

int Tls::bio_read_tout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout)
{
    auto s = static_cast<Tls *>(ctx);
    return s->recv_timeout(buf, len, timeout);
}

int Tls::write(const unsigned char *buf, size_t len)
{
    return mbedtls_ssl_write( &ssl_, buf, len );
}

int Tls::read(unsigned char *buf, size_t len)
{
    return mbedtls_ssl_read( &ssl_, buf, len );
}

bool Tls::set_own_cert(const_buf crt, const_buf key)
{
    int ret = mbedtls_x509_crt_parse(&public_cert_, crt.first, crt.second);
    if (ret < 0) {
        print_error("mbedtls_x509_crt_parse", ret);
        return false;
    }
    ret = mbedtls_pk_parse_key(&pk_key_, key.first, key.second, nullptr, 0);
    if (ret < 0) {
        print_error("mbedtls_pk_parse_keyfile", ret);
        return false;
    }
    return true;
}

bool Tls::set_ca_cert(const_buf crt)
{
    int ret = mbedtls_x509_crt_parse(&ca_cert_, crt.first, crt.second);
    if (ret < 0) {
        print_error("mbedtls_x509_crt_parse", ret);
        return false;
    }
    return true;
}

bool Tls::set_hostname(const char *name)
{
    int ret = mbedtls_ssl_set_hostname(&ssl_, name);
    if (ret < 0) {
        print_error("mbedtls_ssl_set_hostname", ret);
        return false;
    }
    return true;
}

Tls::Tls()
{
    mbedtls_x509_crt_init(&public_cert_);
    mbedtls_pk_init(&pk_key_);
    mbedtls_x509_crt_init(&ca_cert_);
}

int Tls::mbedtls_pk_parse_key(mbedtls_pk_context *ctx, const unsigned char *key, size_t keylen, const unsigned char *pwd, size_t pwdlen)
{

    return ::mbedtls_pk_parse_key(ctx, key, keylen, pwd, pwdlen, nullptr, nullptr);
}

size_t Tls::get_available_bytes()
{
    return ::mbedtls_ssl_get_bytes_avail(&ssl_);
}

Tls::~Tls()
{
    ::mbedtls_ssl_config_free(&conf_);
    ::mbedtls_ssl_free(&ssl_);
    ::mbedtls_pk_free(&pk_key_);
    ::mbedtls_x509_crt_free(&public_cert_);
    ::mbedtls_x509_crt_free(&ca_cert_);
}

bool Tls::get_session()
{
    if (session_ == nullptr) {
        session_ = std::make_unique<unique_session>();
    }
    int ret = ::mbedtls_ssl_get_session(&ssl_, session_->ptr());
    if (ret != 0) {
        print_error("mbedtls_ssl_get_session() failed", ret);
        return false;
    }
    return true;
}

bool Tls::set_session()
{
    if (session_ == nullptr) {
        printf("session hasn't been initialized");
        return false;
    }
    int ret = mbedtls_ssl_set_session(&ssl_, session_->ptr());
    if (ret != 0) {
        print_error("mbedtls_ssl_set_session() failed", ret);
        return false;
    }
    return true;
}

void Tls::reset_session()
{
    session_.reset(nullptr);
}
bool Tls::is_session_loaded()
{
    return session_ != nullptr;
}

#if CONFIG_MBEDTLS_SSL_PROTO_DTLS
bool Tls::init_dtls_cookies()
{
    int ret = mbedtls_ssl_cookie_setup(&cookie_, mbedtls_ctr_drbg_random, &ctr_drbg_);
    if (ret != 0) {
        print_error("mbedtls_ssl_cookie_setup() failed", ret);
        return false;
    }
    mbedtls_ssl_conf_dtls_cookies(&conf_, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check, &cookie_);

    return true;
}

bool Tls::set_client_id()
{
    int ret;
    if (client_id_ == const_buf{}) {
        printf("client_id is not set");
        return false;
    }
    mbedtls_ssl_session_reset(&ssl_);
    if ((ret = mbedtls_ssl_set_client_transport_id(&ssl_, client_id_.first, client_id_.second)) != 0) {
        print_error("mbedtls_ssl_set_client_transport_id()", ret);
        return false;
    }
    return true;
}
#endif
