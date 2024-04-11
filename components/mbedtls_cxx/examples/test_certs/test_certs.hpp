/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "mbedtls_wrap.hpp"

namespace test_certs {
using pem_format = const unsigned char;
extern pem_format cacert_start[] asm("_binary_ca_crt_start");
extern pem_format cacert_end[] asm("_binary_ca_crt_end");
extern pem_format clientcert_start[] asm("_binary_client_crt_start");
extern pem_format clientcert_end[] asm("_binary_client_crt_end");
extern pem_format clientkey_start[] asm("_binary_client_key_start");
extern pem_format clientkey_end[] asm("_binary_client_key_end");
extern pem_format servercert_start[] asm("_binary_srv_crt_start");
extern pem_format servercert_end[] asm("_binary_srv_crt_end");
extern pem_format serverkey_start[] asm("_binary_srv_key_start");
extern pem_format serverkey_end[] asm("_binary_srv_key_end");

enum class type {
    cacert,
    servercert,
    serverkey,
    clientcert,
    clientkey
};

#define IF_BUF_TYPE(buf_type)  \
    if (t == type::buf_type) { \
        return idf::mbedtls_cxx::const_buf{buf_type ## _start, buf_type ## _end - buf_type ## _start}; \
    }

static inline idf::mbedtls_cxx::const_buf get_buf(type t)
{
    IF_BUF_TYPE(cacert);
    IF_BUF_TYPE(servercert);
    IF_BUF_TYPE(serverkey);
    IF_BUF_TYPE(clientcert);
    IF_BUF_TYPE(clientkey);
    return idf::mbedtls_cxx::const_buf{};
}

static inline const char *get_server_cn()
{
    return "espressif.local";
}

}
