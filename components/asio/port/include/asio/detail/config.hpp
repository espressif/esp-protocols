//
// SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: BSL-1.0
//
#pragma once

#include "sys/socket.h"
// sock_utils' socketpair.h only declares the ::socketpair()/::pipe() shims
// that asio's own TU (asio.cpp) calls from socket_ops.ipp. Client code that
// just includes asio.hpp never needs these declarations (asio uses separate
// compilation, so client TUs only see the asio::detail::socket_ops::socketpair
// wrapper declaration, not the POSIX call). Gate the include on ASIO_SOURCE
// -- defined by asio/impl/src.hpp before including this header -- so sock_utils
// stays a private dependency of the asio component.
#if defined(ASIO_SOURCE)
#include "socketpair.h"
#endif
#include "asio_stub.hpp"

#include_next "asio/detail/config.hpp"
