//
// winsock_init.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_WINSOCK_INIT_HPP
#define ASIO_DETAIL_WINSOCK_INIT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

#if defined(_WIN32)

#include "asio/detail/push_options.hpp"
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "asio/detail/pop_options.hpp"

#include "asio/detail/socket_types.hpp"

namespace asio {
namespace detail {

template <int Major = 2, int Minor = 0>
class winsock_init
  : private boost::noncopyable
{
private:
  // Structure to perform the actual initialisation.
  struct do_init
  {
    do_init()
    {
      WSADATA wsa_data;
      ::WSAStartup(MAKEWORD(Major, Minor), &wsa_data);
    }

    ~do_init()
    {
      ::WSACleanup();
    }

    // Helper function to manage a do_init singleton. The static instance of the
    // winsock_init object ensures that this function is always called before
    // main, and therefore before any other threads can get started. The do_init
    // instance must be static in this function to ensure that it gets
    // initialised before any other global objects try to use it.
    static boost::shared_ptr<do_init> instance()
    {
      static boost::shared_ptr<do_init> init(new do_init);
      return init;
    }
  };

public:
  // Constructor.
  winsock_init()
    : ref_(do_init::instance())
  {
    while (&instance_ == 0); // Ensure winsock_init::instance_ is linked in.
  }

  // Destructor.
  ~winsock_init()
  {
  }

private:
  // Instance to force initialisation of winsock at global scope.
  static winsock_init instance_;

  // Reference to singleton do_init object to ensure that winsock does not get
  // cleaned up until the last user has finished with it.
  boost::shared_ptr<do_init> ref_;
};

template <int Major, int Minor>
winsock_init<Major, Minor> winsock_init<Major, Minor>::instance_;

} // namespace detail
} // namespace asio

#endif // _WIN32

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_WINSOCK_INIT_HPP
