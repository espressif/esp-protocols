/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <utility>
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

using  const_buf = std::pair<const unsigned char *, std::size_t>;
using  buf = std::pair<unsigned char *, std::size_t>;

class Tls {
public:
    Tls();
    bool init(bool is_server, bool verify);
    int handshake();
    int write(const unsigned char *buf, size_t len);
    int read(unsigned char *buf, size_t len);
    bool set_own_cert(const_buf crt, const_buf key);
    bool set_ca_cert(const_buf crt);
    virtual int send(const unsigned char *buf, size_t len) = 0;
    virtual int recv(unsigned char *buf, size_t len) = 0;
    size_t get_available_bytes();

private:
    mbedtls_ssl_context ssl_{};
    mbedtls_x509_crt public_cert_{};
    mbedtls_pk_context pk_key_{};
    mbedtls_x509_crt ca_cert_{};
    mbedtls_ssl_config conf_{};
    mbedtls_ctr_drbg_context ctr_drbg_{};
    mbedtls_entropy_context entropy_{};

    static void print_error(const char *function, int error_code);
    static int bio_write(void *ctx, const unsigned char *buf, size_t len);
    static int bio_read(void *ctx, unsigned char *buf, size_t len);
    int mbedtls_pk_parse_key( mbedtls_pk_context *ctx,
                              const unsigned char *key, size_t keylen,
                              const unsigned char *pwd, size_t pwdlen);

};
