//
// strand.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/strand.hpp"

#include <sstream>
#include "asio/io_service.hpp"
#include "asio/post.hpp"
#include "asio/thread.hpp"
#include "unit_test.hpp"

#if defined(ASIO_HAS_BOOST_DATE_TIME)
# include "asio/deadline_timer.hpp"
#else // defined(ASIO_HAS_BOOST_DATE_TIME)
# include "asio/steady_timer.hpp"
#endif // defined(ASIO_HAS_BOOST_DATE_TIME)

#if defined(ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(ASIO_HAS_BOOST_BIND)

using namespace asio;

#if defined(ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
namespace bindns = std;
#endif

#if defined(ASIO_HAS_BOOST_DATE_TIME)
typedef deadline_timer timer;
namespace chronons = boost::posix_time;
#elif defined(ASIO_HAS_STD_CHRONO)
typedef steady_timer timer;
namespace chronons = std::chrono;
#elif defined(ASIO_HAS_BOOST_CHRONO)
typedef steady_timer timer;
namespace chronons = boost::chrono;
#endif // defined(ASIO_HAS_BOOST_DATE_TIME)

void increment(int* count)
{
  ++(*count);
}

void increment_without_lock(io_service::strand* s, int* count)
{
  ASIO_CHECK(!s->running_in_this_thread());

  int original_count = *count;

  s->dispatch(bindns::bind(increment, count));

  // No other functions are currently executing through the locking dispatcher,
  // so the previous call to dispatch should have successfully nested.
  ASIO_CHECK(*count == original_count + 1);
}

void increment_with_lock(io_service::strand* s, int* count)
{
  ASIO_CHECK(s->running_in_this_thread());

  int original_count = *count;

  s->dispatch(bindns::bind(increment, count));

  // The current function already holds the strand's lock, so the
  // previous call to dispatch should have successfully nested.
  ASIO_CHECK(*count == original_count + 1);
}

void sleep_increment(io_service* ios, int* count)
{
  timer t(*ios, chronons::seconds(2));
  t.wait();

  ++(*count);
}

void start_sleep_increments(io_service* ios, io_service::strand* s, int* count)
{
  // Give all threads a chance to start.
  timer t(*ios, chronons::seconds(2));
  t.wait();

  // Start three increments.
  s->post(bindns::bind(sleep_increment, ios, count));
  s->post(bindns::bind(sleep_increment, ios, count));
  s->post(bindns::bind(sleep_increment, ios, count));
}

void throw_exception()
{
  throw 1;
}

void io_service_run(io_service* ios)
{
  ios->run();
}

void strand_test()
{
  io_service ios;
  io_service::strand s(ios);
  int count = 0;

  post(ios, bindns::bind(increment_without_lock, &s, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(count == 0);

  ios.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(count == 1);

  count = 0;
  ios.restart();
  post(s, bindns::bind(increment_with_lock, &s, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(count == 0);

  ios.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(count == 1);

  count = 0;
  ios.restart();
  post(ios, bindns::bind(start_sleep_increments, &ios, &s, &count));
  thread thread1(bindns::bind(io_service_run, &ios));
  thread thread2(bindns::bind(io_service_run, &ios));

  // Check all events run one after another even though there are two threads.
  timer timer1(ios, chronons::seconds(3));
  timer1.wait();
  ASIO_CHECK(count == 0);
#if defined(ASIO_HAS_BOOST_DATE_TIME)
  timer1.expires_at(timer1.expires_at() + chronons::seconds(2));
#else // defined(ASIO_HAS_BOOST_DATE_TIME)
  timer1.expires_at(timer1.expiry() + chronons::seconds(2));
#endif // defined(ASIO_HAS_BOOST_DATE_TIME)
  timer1.wait();
  ASIO_CHECK(count == 1);
#if defined(ASIO_HAS_BOOST_DATE_TIME)
  timer1.expires_at(timer1.expires_at() + chronons::seconds(2));
#else // defined(ASIO_HAS_BOOST_DATE_TIME)
  timer1.expires_at(timer1.expiry() + chronons::seconds(2));
#endif // defined(ASIO_HAS_BOOST_DATE_TIME)
  timer1.wait();
  ASIO_CHECK(count == 2);

  thread1.join();
  thread2.join();

  // The run() calls will not return until all work has finished.
  ASIO_CHECK(count == 3);

  count = 0;
  int exception_count = 0;
  ios.restart();
  post(s, throw_exception);
  post(s, bindns::bind(increment, &count));
  post(s, bindns::bind(increment, &count));
  post(s, throw_exception);
  post(s, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(count == 0);
  ASIO_CHECK(exception_count == 0);

  for (;;)
  {
    try
    {
      ios.run();
      break;
    }
    catch (int)
    {
      ++exception_count;
    }
  }

  // The run() calls will not return until all work has finished.
  ASIO_CHECK(count == 3);
  ASIO_CHECK(exception_count == 2);

  count = 0;
  ios.restart();

  // Check for clean shutdown when handlers posted through an orphaned strand
  // are abandoned.
  {
    io_service::strand s2(ios);
    post(s2, bindns::bind(increment, &count));
    post(s2, bindns::bind(increment, &count));
    post(s2, bindns::bind(increment, &count));
  }

  // No handlers can be called until run() is called.
  ASIO_CHECK(count == 0);
}

ASIO_TEST_SUITE
(
  "strand",
  ASIO_TEST_CASE(strand_test)
)
