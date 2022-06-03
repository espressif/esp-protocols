//
// tcp.cpp
// ~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Enable cancel() support on Windows.
#define ASIO_ENABLE_CANCELIO 1

// Test that header file is self-contained.
#include "asio/ip/tcp.hpp"

#include <boost/bind.hpp>
#include <cstring>
#include "asio/io_service.hpp"
#include "asio/placeholders.hpp"
#include "asio/read.hpp"
#include "asio/write.hpp"
#include "../unit_test.hpp"
#include "../archetypes/io_control_command.hpp"

//------------------------------------------------------------------------------

// ip_tcp_compile test
// ~~~~~~~~~~~~~~~~~~~
// The following test checks that all nested classes, enums and constants in
// ip::tcp compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_compile {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_service ios;
    ip::tcp::socket sock(ios);

    // no_delay class.

    ip::tcp::no_delay no_delay1(true);
    sock.set_option(no_delay1);
    ip::tcp::no_delay no_delay2;
    sock.get_option(no_delay2);
    no_delay1 = true;
    (void)static_cast<bool>(no_delay1);
    (void)static_cast<bool>(!no_delay1);
    (void)static_cast<bool>(no_delay1.value());
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_compile

//------------------------------------------------------------------------------

// ip_tcp_runtime test
// ~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::tcp class.

namespace ip_tcp_runtime {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  io_service ios;
  ip::tcp::socket sock(ios, ip::tcp::v4());
  asio::error_code ec;

  // no_delay class.

  ip::tcp::no_delay no_delay1(true);
  BOOST_CHECK(no_delay1.value());
  BOOST_CHECK(static_cast<bool>(no_delay1));
  BOOST_CHECK(!!no_delay1);
  sock.set_option(no_delay1, ec);
  BOOST_CHECK(!ec);

  ip::tcp::no_delay no_delay2;
  sock.get_option(no_delay2, ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(no_delay2.value());
  BOOST_CHECK(static_cast<bool>(no_delay2));
  BOOST_CHECK(!!no_delay2);

  ip::tcp::no_delay no_delay3(false);
  BOOST_CHECK(!no_delay3.value());
  BOOST_CHECK(!static_cast<bool>(no_delay3));
  BOOST_CHECK(!no_delay3);
  sock.set_option(no_delay3, ec);
  BOOST_CHECK(!ec);

  ip::tcp::no_delay no_delay4;
  sock.get_option(no_delay4, ec);
  BOOST_CHECK(!ec);
  BOOST_CHECK(!no_delay4.value());
  BOOST_CHECK(!static_cast<bool>(no_delay4));
  BOOST_CHECK(!no_delay4);
}

} // namespace ip_tcp_runtime

//------------------------------------------------------------------------------

// ip_tcp_socket_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::tcp::socket compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_socket_compile {

void connect_handler(const asio::error_code&)
{
}

void send_handler(const asio::error_code&, std::size_t)
{
}

void receive_handler(const asio::error_code&, std::size_t)
{
}

void write_some_handler(const asio::error_code&, std::size_t)
{
}

void read_some_handler(const asio::error_code&, std::size_t)
{
}

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_service ios;
    char mutable_char_buffer[128] = "";
    const char const_char_buffer[128] = "";
    socket_base::message_flags in_flags = 0;
    socket_base::keep_alive socket_option;
    archetypes::io_control_command io_control_command;
    asio::error_code ec;

    // basic_stream_socket constructors.

    ip::tcp::socket socket1(ios);
    ip::tcp::socket socket2(ios, ip::tcp::v4());
    ip::tcp::socket socket3(ios, ip::tcp::v6());
    ip::tcp::socket socket4(ios, ip::tcp::endpoint(ip::tcp::v4(), 0));
    ip::tcp::socket socket5(ios, ip::tcp::endpoint(ip::tcp::v6(), 0));
    int native_socket1 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ip::tcp::socket socket6(ios, ip::tcp::v4(), native_socket1);

    // basic_io_object functions.

    io_service& ios_ref = socket1.io_service();
    (void)ios_ref;

    // basic_socket functions.

    ip::tcp::socket::lowest_layer_type& lowest_layer = socket1.lowest_layer();
    (void)lowest_layer;

    const ip::tcp::socket& socket7 = socket1;
    const ip::tcp::socket::lowest_layer_type& lowest_layer2
      = socket7.lowest_layer();
    (void)lowest_layer2;

    socket1.open(ip::tcp::v4());
    socket1.open(ip::tcp::v6());
    socket1.open(ip::tcp::v4(), ec);
    socket1.open(ip::tcp::v6(), ec);

    int native_socket2 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    socket1.assign(ip::tcp::v4(), native_socket2);
    int native_socket3 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    socket1.assign(ip::tcp::v4(), native_socket3, ec);

    bool is_open = socket1.is_open();
    (void)is_open;

    socket1.close();
    socket1.close(ec);

    ip::tcp::socket::native_type native_socket4 = socket1.native();
    (void)native_socket4;

    socket1.cancel();
    socket1.cancel(ec);

    bool at_mark1 = socket1.at_mark();
    (void)at_mark1;
    bool at_mark2 = socket1.at_mark(ec);
    (void)at_mark2;

    std::size_t available1 = socket1.available();
    (void)available1;
    std::size_t available2 = socket1.available(ec);
    (void)available2;

    socket1.bind(ip::tcp::endpoint(ip::tcp::v4(), 0));
    socket1.bind(ip::tcp::endpoint(ip::tcp::v6(), 0));
    socket1.bind(ip::tcp::endpoint(ip::tcp::v4(), 0), ec);
    socket1.bind(ip::tcp::endpoint(ip::tcp::v6(), 0), ec);

    socket1.connect(ip::tcp::endpoint(ip::tcp::v4(), 0));
    socket1.connect(ip::tcp::endpoint(ip::tcp::v6(), 0));
    socket1.connect(ip::tcp::endpoint(ip::tcp::v4(), 0), ec);
    socket1.connect(ip::tcp::endpoint(ip::tcp::v6(), 0), ec);

    socket1.async_connect(ip::tcp::endpoint(ip::tcp::v4(), 0), connect_handler);
    socket1.async_connect(ip::tcp::endpoint(ip::tcp::v6(), 0), connect_handler);

    socket1.set_option(socket_option);
    socket1.set_option(socket_option, ec);

    socket1.get_option(socket_option);
    socket1.get_option(socket_option, ec);

    socket1.io_control(io_control_command);
    socket1.io_control(io_control_command, ec);

    ip::tcp::endpoint endpoint1 = socket1.local_endpoint();
    ip::tcp::endpoint endpoint2 = socket1.local_endpoint(ec);

    ip::tcp::endpoint endpoint3 = socket1.remote_endpoint();
    ip::tcp::endpoint endpoint4 = socket1.remote_endpoint(ec);

    socket1.shutdown(socket_base::shutdown_both);
    socket1.shutdown(socket_base::shutdown_both, ec);

    // basic_stream_socket functions.

    socket1.send(buffer(mutable_char_buffer));
    socket1.send(buffer(const_char_buffer));
    socket1.send(null_buffers());
    socket1.send(buffer(mutable_char_buffer), in_flags);
    socket1.send(buffer(const_char_buffer), in_flags);
    socket1.send(null_buffers(), in_flags);
    socket1.send(buffer(mutable_char_buffer), in_flags, ec);
    socket1.send(buffer(const_char_buffer), in_flags, ec);
    socket1.send(null_buffers(), in_flags, ec);

    socket1.async_send(buffer(mutable_char_buffer), send_handler);
    socket1.async_send(buffer(const_char_buffer), send_handler);
    socket1.async_send(null_buffers(), send_handler);
    socket1.async_send(buffer(mutable_char_buffer), in_flags, send_handler);
    socket1.async_send(buffer(const_char_buffer), in_flags, send_handler);
    socket1.async_send(null_buffers(), in_flags, send_handler);

    socket1.receive(buffer(mutable_char_buffer));
    socket1.receive(null_buffers());
    socket1.receive(buffer(mutable_char_buffer), in_flags);
    socket1.receive(null_buffers(), in_flags);
    socket1.receive(buffer(mutable_char_buffer), in_flags, ec);
    socket1.receive(null_buffers(), in_flags, ec);

    socket1.async_receive(buffer(mutable_char_buffer), receive_handler);
    socket1.async_receive(null_buffers(), receive_handler);
    socket1.async_receive(buffer(mutable_char_buffer), in_flags,
        receive_handler);
    socket1.async_receive(null_buffers(), in_flags, receive_handler);

    socket1.write_some(buffer(mutable_char_buffer));
    socket1.write_some(buffer(const_char_buffer));
    socket1.write_some(null_buffers());
    socket1.write_some(buffer(mutable_char_buffer), ec);
    socket1.write_some(buffer(const_char_buffer), ec);
    socket1.write_some(null_buffers(), ec);

    socket1.async_write_some(buffer(mutable_char_buffer), write_some_handler);
    socket1.async_write_some(buffer(const_char_buffer), write_some_handler);
    socket1.async_write_some(null_buffers(), write_some_handler);

    socket1.read_some(buffer(mutable_char_buffer));
    socket1.read_some(buffer(mutable_char_buffer), ec);
    socket1.read_some(null_buffers(), ec);

    socket1.async_read_some(buffer(mutable_char_buffer), read_some_handler);
    socket1.async_read_some(null_buffers(), read_some_handler);
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_socket_compile

//------------------------------------------------------------------------------

// ip_tcp_socket_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::tcp::socket class.

namespace ip_tcp_socket_runtime {

static const char write_data[]
  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void handle_read_noop(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  BOOST_CHECK(!err);
  BOOST_CHECK(bytes_transferred == 0);
}

void handle_write_noop(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  BOOST_CHECK(!err);
  BOOST_CHECK(bytes_transferred == 0);
}

void handle_read(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  BOOST_CHECK(!err);
  BOOST_CHECK(bytes_transferred == sizeof(write_data));
}

void handle_write(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  BOOST_CHECK(!err);
  BOOST_CHECK(bytes_transferred == sizeof(write_data));
}

void handle_read_cancel(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  BOOST_CHECK(err == asio::error::operation_aborted);
  BOOST_CHECK(bytes_transferred == 0);
}

void handle_read_eof(const asio::error_code& err,
    size_t bytes_transferred, bool* called)
{
  *called = true;
  BOOST_CHECK(err == asio::error::eof);
  BOOST_CHECK(bytes_transferred == 0);
}

void test()
{
  using namespace std; // For memcmp.
  using namespace asio;
  namespace ip = asio::ip;

  io_service ios;

  ip::tcp::acceptor acceptor(ios, ip::tcp::endpoint(ip::tcp::v4(), 0));
  ip::tcp::endpoint server_endpoint = acceptor.local_endpoint();
  server_endpoint.address(ip::address_v4::loopback());

  ip::tcp::socket client_side_socket(ios);
  ip::tcp::socket server_side_socket(ios);

  client_side_socket.connect(server_endpoint);
  acceptor.accept(server_side_socket);

  // No-op read.

  bool read_noop_completed = false;
  client_side_socket.async_read_some(
      asio::mutable_buffers_1(0, 0),
      boost::bind(handle_read_noop,
        asio::placeholders::error,
        asio::placeholders::bytes_transferred,
        &read_noop_completed));

  ios.run();
  BOOST_CHECK(read_noop_completed);

  // No-op write.

  bool write_noop_completed = false;
  client_side_socket.async_write_some(
      asio::const_buffers_1(0, 0),
      boost::bind(handle_write_noop,
        asio::placeholders::error,
        asio::placeholders::bytes_transferred,
        &write_noop_completed));

  ios.reset();
  ios.run();
  BOOST_CHECK(write_noop_completed);

  // Read and write to transfer data.

  char read_buffer[sizeof(write_data)];
  bool read_completed = false;
  asio::async_read(client_side_socket,
      asio::buffer(read_buffer),
      boost::bind(handle_read,
        asio::placeholders::error,
        asio::placeholders::bytes_transferred,
        &read_completed));

  bool write_completed = false;
  asio::async_write(server_side_socket,
      asio::buffer(write_data),
      boost::bind(handle_write,
        asio::placeholders::error,
        asio::placeholders::bytes_transferred,
        &write_completed));

  ios.reset();
  ios.run();
  BOOST_CHECK(read_completed);
  BOOST_CHECK(write_completed);
  BOOST_CHECK(memcmp(read_buffer, write_data, sizeof(write_data)) == 0);

  // Cancelled read.

  bool read_cancel_completed = false;
  asio::async_read(server_side_socket,
      asio::buffer(read_buffer),
      boost::bind(handle_read_cancel,
        asio::placeholders::error,
        asio::placeholders::bytes_transferred,
        &read_cancel_completed));

  ios.reset();
  ios.poll();
  BOOST_CHECK(!read_cancel_completed);

  server_side_socket.cancel();

  ios.reset();
  ios.run();
  BOOST_CHECK(read_cancel_completed);

  // A read when the peer closes socket should fail with eof.

  bool read_eof_completed = false;
  asio::async_read(client_side_socket,
      asio::buffer(read_buffer),
      boost::bind(handle_read_eof,
        asio::placeholders::error,
        asio::placeholders::bytes_transferred,
        &read_eof_completed));

  server_side_socket.close();

  ios.reset();
  ios.run();
  BOOST_CHECK(read_eof_completed);
}

} // namespace ip_tcp_socket_runtime

//------------------------------------------------------------------------------

// ip_tcp_acceptor_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::tcp::acceptor
// class.

namespace ip_tcp_acceptor_runtime {

void handle_accept(const asio::error_code& err)
{
  BOOST_CHECK(!err);
}

void handle_connect(const asio::error_code& err)
{
  BOOST_CHECK(!err);
}

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  io_service ios;

  ip::tcp::acceptor acceptor(ios, ip::tcp::endpoint(ip::tcp::v4(), 0));
  ip::tcp::endpoint server_endpoint = acceptor.local_endpoint();
  server_endpoint.address(ip::address_v4::loopback());

  ip::tcp::socket client_side_socket(ios);
  ip::tcp::socket server_side_socket(ios);

  client_side_socket.connect(server_endpoint);
  acceptor.accept(server_side_socket);

  client_side_socket.close();
  server_side_socket.close();

  client_side_socket.connect(server_endpoint);
  ip::tcp::endpoint client_endpoint;
  acceptor.accept(server_side_socket, client_endpoint);

  ip::tcp::endpoint client_side_local_endpoint
    = client_side_socket.local_endpoint();
  BOOST_CHECK(client_side_local_endpoint.port() == client_endpoint.port());

  ip::tcp::endpoint server_side_remote_endpoint
    = server_side_socket.remote_endpoint();
  BOOST_CHECK(server_side_remote_endpoint.port() == client_endpoint.port());

  client_side_socket.close();
  server_side_socket.close();

  acceptor.async_accept(server_side_socket, handle_accept);
  client_side_socket.async_connect(server_endpoint, handle_connect);

  ios.run();

  client_side_socket.close();
  server_side_socket.close();

  acceptor.async_accept(server_side_socket, client_endpoint, handle_accept);
  client_side_socket.async_connect(server_endpoint, handle_connect);

  ios.reset();
  ios.run();

  client_side_local_endpoint = client_side_socket.local_endpoint();
  BOOST_CHECK(client_side_local_endpoint.port() == client_endpoint.port());

  server_side_remote_endpoint = server_side_socket.remote_endpoint();
  BOOST_CHECK(server_side_remote_endpoint.port() == client_endpoint.port());
}

} // namespace ip_tcp_acceptor_runtime

//------------------------------------------------------------------------------

// ip_tcp_resolver_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::tcp::resolver compile and link correctly. Runtime failures are ignored.

namespace ip_tcp_resolver_compile {

void resolve_handler(const asio::error_code&,
    asio::ip::tcp::resolver::iterator)
{
}

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_service ios;
    asio::error_code ec;
    ip::tcp::resolver::query q(ip::tcp::v4(), "localhost", "0");
    ip::tcp::endpoint e(ip::address_v4::loopback(), 0);

    // basic_resolver constructors.

    ip::tcp::resolver resolver(ios);

    // basic_io_object functions.

    io_service& ios_ref = resolver.io_service();
    (void)ios_ref;

    // basic_resolver functions.

    resolver.cancel();

    ip::tcp::resolver::iterator iter1 = resolver.resolve(q);
    (void)iter1;

    ip::tcp::resolver::iterator iter2 = resolver.resolve(q, ec);
    (void)iter2;

    ip::tcp::resolver::iterator iter3 = resolver.resolve(e);
    (void)iter3;

    ip::tcp::resolver::iterator iter4 = resolver.resolve(e, ec);
    (void)iter4;

    resolver.async_resolve(q, resolve_handler);

    resolver.async_resolve(e, resolve_handler);
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_tcp_resolver_compile

//------------------------------------------------------------------------------

test_suite* init_unit_test_suite(int, char*[])
{
  test_suite* test = BOOST_TEST_SUITE("ip/tcp");
  test->add(BOOST_TEST_CASE(&ip_tcp_compile::test));
  test->add(BOOST_TEST_CASE(&ip_tcp_runtime::test));
  test->add(BOOST_TEST_CASE(&ip_tcp_socket_compile::test));
  test->add(BOOST_TEST_CASE(&ip_tcp_socket_runtime::test));
  test->add(BOOST_TEST_CASE(&ip_tcp_acceptor_runtime::test));
  test->add(BOOST_TEST_CASE(&ip_tcp_resolver_compile::test));
  return test;
}
