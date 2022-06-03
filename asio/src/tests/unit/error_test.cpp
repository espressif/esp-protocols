//
// error_test.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
#include "asio/error.hpp"

#include <sstream>
#include "unit_test.hpp"

void test_error_code(int code)
{
  asio::error error(code);
  BOOST_CHECK(code == error.code());
  BOOST_CHECK(error.what() != 0);

  BOOST_CHECK(code == 0 || error);
  BOOST_CHECK(code == 0 || !!error);

  asio::error error2(error);
  BOOST_CHECK(error == error2);
  BOOST_CHECK(!(error != error2));

  asio::error error3;
  error3 = error;
  BOOST_CHECK(error == error3);
  BOOST_CHECK(!(error != error3));

  std::ostringstream os;
  os << error;
  BOOST_CHECK(os.str() == error.what());
}

void error_test()
{
  test_error_code(asio::error::access_denied);
  test_error_code(asio::error::address_family_not_supported);
  test_error_code(asio::error::address_in_use);
  test_error_code(asio::error::already_connected);
  test_error_code(asio::error::already_started);
  test_error_code(asio::error::connection_aborted);
  test_error_code(asio::error::connection_refused);
  test_error_code(asio::error::connection_reset);
  test_error_code(asio::error::bad_descriptor);
  test_error_code(asio::error::eof);
  test_error_code(asio::error::fault);
  test_error_code(asio::error::host_not_found);
  test_error_code(asio::error::host_not_found_try_again);
  test_error_code(asio::error::host_unreachable);
  test_error_code(asio::error::in_progress);
  test_error_code(asio::error::interrupted);
  test_error_code(asio::error::invalid_argument);
  test_error_code(asio::error::message_size);
  test_error_code(asio::error::network_down);
  test_error_code(asio::error::network_reset);
  test_error_code(asio::error::network_unreachable);
  test_error_code(asio::error::no_descriptors);
  test_error_code(asio::error::no_buffer_space);
  test_error_code(asio::error::no_host_data);
  test_error_code(asio::error::no_memory);
  test_error_code(asio::error::no_permission);
  test_error_code(asio::error::no_protocol_option);
  test_error_code(asio::error::no_recovery);
  test_error_code(asio::error::not_connected);
  test_error_code(asio::error::not_socket);
  test_error_code(asio::error::not_supported);
  test_error_code(asio::error::operation_aborted);
  test_error_code(asio::error::shut_down);
  test_error_code(asio::error::success);
  test_error_code(asio::error::timed_out);
  test_error_code(asio::error::try_again);
  test_error_code(asio::error::would_block);
}

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  test_suite* test = BOOST_TEST_SUITE("error");
  test->add(BOOST_TEST_CASE(&error_test));
  return test;
}
