//
// SPDX-FileCopyrightText: 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// SPDX-License-Identifier: BSL-1.0
//
// SPDX-FileContributor: 2024 Espressif Systems (Shanghai) CO LTD
//
#include "asio/detail/posix_event.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/error.hpp"
#include "asio/detail/push_options.hpp"
#include <unistd.h>
#include <climits>

namespace asio::detail {
// This replaces asio's posix_event constructor
// since the default POSIX version uses pthread_condattr_t operations (init, setclock, destroy),
// which are not available on all IDF versions (some are defined in compilers' headers, others in
// pthread library, but they typically return `ENOSYS` which causes trouble in the event wrapper)
// IMPORTANT: Check implementation of posix_event() when upgrading upstream asio in order not to
// miss any initialization step.
posix_event::posix_event()
    : state_(0)
{
    int error = ::pthread_cond_init(&cond_, nullptr);
    asio::error_code ec(error, asio::error::get_system_category());
    asio::detail::throw_error(ec, "event");
}
} // namespace asio::detail

extern "C" int pause(void)
{
    while (true) {
        ::sleep(UINT_MAX);
    }
}
