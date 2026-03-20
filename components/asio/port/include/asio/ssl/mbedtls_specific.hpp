//
// SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: BSL-1.0
//

#pragma once

#include "asio/ssl/context_base.hpp"
#include "asio/ssl/context.hpp"
#include "asio/ssl/detail/openssl_types.hpp"

namespace asio {
namespace ssl {
namespace mbedtls {

/**
 * @brief Configures specific hostname to be used in peer verification
 *
 * @param handle asio::ssl context handle type
 * @param name hostname to be verified (std::string ownership will be moved to ssl::context)
 *
 * @return true on success
 */
bool set_hostname(asio::ssl::context::native_handle_type handle, std::string name);

};
};
} // namespace asio::ssl::mbedtls
