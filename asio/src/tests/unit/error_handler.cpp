//
// error_handler_test.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2006 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/error_handler.hpp"

#include <sstream>
#include "asio.hpp"
#include "unit_test.hpp"

using namespace asio;

void error_handler_test()
{
  io_service ios;

  ip::tcp::socket s(ios);
  ip::tcp::endpoint endpoint(ip::tcp::v4(), 321);

  error expected_err;
  s.connect(endpoint, assign_error(expected_err));
  s.close();

  try
  {
    s.close();
    s.connect(endpoint, throw_error());
    BOOST_CHECK(0);
  }
  catch (error&)
  {
  }

  try
  {
    s.close();
    s.connect(endpoint, ignore_error());
  }
  catch (error&)
  {
    BOOST_CHECK(0);
  }

  s.close();
  error err;
  s.connect(endpoint, assign_error(err));
  BOOST_CHECK(err == expected_err);
}

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  test_suite* test = BOOST_TEST_SUITE("error_handler");
  test->add(BOOST_TEST_CASE(&error_handler_test));
  return test;
}
