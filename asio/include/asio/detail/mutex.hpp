//
// mutex.hpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_MUTEX_HPP
#define ASIO_DETAIL_MUTEX_HPP

#include "asio/detail/push_options.hpp"

#include "asio/detail/posix_mutex.hpp"
#include "asio/detail/win_mutex.hpp"

namespace asio {
namespace detail {

#if defined(_WIN32)
typedef win_mutex mutex;
#else
typedef posix_mutex mutex;
#endif

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_MUTEX_HPP
