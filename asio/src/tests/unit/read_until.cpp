//
// read_until.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/read_until.hpp"

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <cstring>
#include "asio.hpp"
#include "unit_test.hpp"

class test_stream
  : private boost::noncopyable
{
public:
  typedef asio::io_service io_service_type;

  test_stream(asio::io_service& io_service)
    : io_service_(io_service),
      length_(0),
      position_(0),
      next_read_length_(0)
  {
  }

  io_service_type& io_service()
  {
    return io_service_;
  }

  void reset(const void* data, size_t length)
  {
    using namespace std; // For memcpy.

    BOOST_CHECK(length <= max_length);

    memcpy(data_, data, length);
    length_ = length;
    position_ = 0;
    next_read_length_ = length;
  }

  void next_read_length(size_t length)
  {
    next_read_length_ = length;
  }

  template <typename Mutable_Buffers>
  size_t read_some(const Mutable_Buffers& buffers)
  {
    using namespace std; // For memcpy.

    size_t total_length = 0;

    typename Mutable_Buffers::const_iterator iter = buffers.begin();
    typename Mutable_Buffers::const_iterator end = buffers.end();
    for (; iter != end && total_length < next_read_length_; ++iter)
    {
      size_t length = asio::buffer_size(*iter);
      if (length > length_ - position_)
        length = length_ - position_;

      if (length > next_read_length_ - total_length)
        length = next_read_length_ - total_length;

      memcpy(asio::buffer_cast<void*>(*iter), data_ + position_, length);
      position_ += length;
      total_length += length;
    }

    return total_length;
  }

  template <typename Mutable_Buffers>
  size_t read_some(const Mutable_Buffers& buffers,
      asio::error_code& ec)
  {
    ec = asio::error_code();
    return read_some(buffers);
  }

  template <typename Mutable_Buffers, typename Handler>
  void async_read_some(const Mutable_Buffers& buffers, Handler handler)
  {
    size_t bytes_transferred = read_some(buffers);
    io_service_.post(asio::detail::bind_handler(
          handler, asio::error_code(), bytes_transferred));
  }

private:
  io_service_type& io_service_;
  enum { max_length = 8192 };
  char data_[max_length];
  size_t length_;
  size_t position_;
  size_t next_read_length_;
};

static const char read_data[]
  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void test_char_read_until()
{
  asio::io_service ios;
  test_stream s(ios);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;

  s.reset(read_data, sizeof(read_data));
  std::size_t length = asio::read_until(s, sb1, 'Z');
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  length = asio::read_until(s, sb1, 'Z');
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  length = asio::read_until(s, sb1, 'Z');
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  length = asio::read_until(s, sb1, 'Z', ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  length = asio::read_until(s, sb1, 'Z', ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  length = asio::read_until(s, sb1, 'Z', ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  length = asio::read_until(s, sb2, 'Z', ec);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  length = asio::read_until(s, sb2, 'Z', ec);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  length = asio::read_until(s, sb2, 'Z', ec);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);
}

void test_string_read_until()
{
  asio::io_service ios;
  test_stream s(ios);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;

  s.reset(read_data, sizeof(read_data));
  std::size_t length = asio::read_until(s, sb1, "XYZ");
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  length = asio::read_until(s, sb1, "XYZ");
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  length = asio::read_until(s, sb1, "XYZ");
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  length = asio::read_until(s, sb1, "XYZ", ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  length = asio::read_until(s, sb1, "XYZ", ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  length = asio::read_until(s, sb1, "XYZ", ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  length = asio::read_until(s, sb2, "XYZ", ec);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  length = asio::read_until(s, sb2, "XYZ", ec);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  length = asio::read_until(s, sb2, "XYZ", ec);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);
}

void async_read_handler(
    const asio::error_code& err, asio::error_code* err_out,
    std::size_t bytes_transferred, std::size_t* bytes_out, bool* called)
{
  *err_out = err;
  *bytes_out = bytes_transferred;
  *called = true;
}

void test_char_async_read_until()
{
  asio::io_service ios;
  test_stream s(ios);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb1, 'Z',
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb1, 'Z',
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb1, 'Z',
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb2, 'Z',
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb2, 'Z',
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb2, 'Z',
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);
}

void test_string_async_read_until()
{
  asio::io_service ios;
  test_stream s(ios);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb1, "XYZ",
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb1, "XYZ",
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb1, "XYZ",
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(!ec);
  BOOST_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb2, "XYZ",
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb2, "XYZ",
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  asio::async_read_until(s, sb2, "XYZ",
      boost::bind(async_read_handler, asio::placeholders::error, &ec,
        asio::placeholders::bytes_transferred, &length, &called));
  ios.reset();
  ios.run();
  BOOST_CHECK(called);
  BOOST_CHECK(ec == asio::error::not_found);
  BOOST_CHECK(length == 0);
}

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  test_suite* test = BOOST_TEST_SUITE("read_until");
  test->add(BOOST_TEST_CASE(&test_char_read_until));
  test->add(BOOST_TEST_CASE(&test_string_read_until));
  test->add(BOOST_TEST_CASE(&test_char_async_read_until));
  test->add(BOOST_TEST_CASE(&test_string_async_read_until));
  return test;
}
