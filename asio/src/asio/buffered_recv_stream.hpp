//
// buffered_recv_stream.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Permission to use, copy, modify, distribute and sell this software and its
// documentation for any purpose is hereby granted without fee, provided that
// the above copyright notice appears in all copies and that both the copyright
// notice and this permission notice appear in supporting documentation. This
// software is provided "as is" without express or implied warranty, and with
// no claim as to its suitability for any purpose.
//

#ifndef ASIO_BUFFERED_RECV_STREAM_HPP
#define ASIO_BUFFERED_RECV_STREAM_HPP

#include <boost/noncopyable.hpp>
#include <boost/type_traits.hpp>
#include "asio/completion_context.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {

/// The buffered_recv_stream class template can be used to add buffering to the
/// recv-related operations of a stream.
template <typename Next_Layer>
class buffered_recv_stream
  : private boost::noncopyable
{
public:
  /// Construct, passing the specified argument to initialise the next layer.
  template <typename Arg>
  explicit buffered_recv_stream(Arg& a)
    : next_layer_(a)
  {
  }

  /// The type of the next layer.
  typedef typename boost::remove_reference<Next_Layer>::type next_layer_type;

  /// Get a reference to the next layer.
  next_layer_type& next_layer()
  {
    return next_layer_;
  }

  /// The type of the lowest layer.
  typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

  /// Get a reference to the lowest layer.
  lowest_layer_type& lowest_layer()
  {
    return next_layer_.lowest_layer();
  }

  /// Close the stream.
  void close()
  {
    next_layer_.close();
  }

  /// Send the given data to the peer. Returns the number of bytes sent or 0 if
  /// the stream was closed cleanly. Throws an exception on failure.
  size_t send(const void* data, size_t length)
  {
    return next_layer_.send(data, length);
  }

  /// The handler when a send operation is completed.
  typedef typename next_layer_type::send_handler send_handler;

  /// Start an asynchronous send. The data being sent must be valid for the
  /// lifetime of the asynchronous operation.
  void async_send(const void* data, size_t length,
      const send_handler& handler,
      completion_context& context = completion_context::null())
  {
    next_layer_.async_send(data, length, handler, context);
  }

  /// Send all of the given data to the peer before returning. Returns the
  /// number of bytes sent on the last send or 0 if the stream was closed
  /// cleanly. Throws an exception on failure.
  size_t send_n(const void* data, size_t length, size_t* total_bytes_sent = 0)
  {
    return next_layer_.send_n(data, length, total_bytes_sent);
  }

  /// The handler when a send_n operation is completed.
  typedef typename next_layer_type::send_n_handler send_n_handler;

  /// Start an asynchronous send that will not return until all of the data has
  /// been sent or an error occurs. The data being sent must be valid for the
  /// lifetime of the asynchronous operation.
  void async_send_n(const void* data, size_t length,
      const send_n_handler& handler,
      completion_context& context = completion_context::null())
  {
    next_layer_.async_send_n(data, length, handler, context);
  }

  /// Receive some data from the peer. Returns the number of bytes received or
  /// 0 if the stream was closed cleanly. Throws an exception on failure.
  size_t recv(void* data, size_t max_length)
  {
    return next_layer_.recv(data, max_length);
  }

  /// The handler when a recv operation is completed.
  typedef typename next_layer_type::recv_handler recv_handler;

  /// Start an asynchronous receive. The buffer for the data being received
  /// must be valid for the lifetime of the asynchronous operation.
  void async_recv(void* data, size_t max_length,
      const recv_handler& handler,
      completion_context& context = completion_context::null())
  {
    next_layer_.async_recv(data, max_length, handler, context);
  }

  /// Receive the specified amount of data from the peer. Returns the number of
  /// bytes received on the last recv call or 0 if the stream was closed
  /// cleanly. Throws an exception on failure.
  size_t recv_n(void* data, size_t length, size_t* total_bytes_recvd = 0)
  {
    return next_layer_.recv_n(data, length, total_bytes_recvd);
  }

  /// The handler when a recv_n operation is completed.
  typedef typename next_layer_type::recv_n_handler recv_n_handler;

  /// Start an asynchronous receive that will not return until the specified
  /// number of bytes has been received or an error occurs. The buffer for the
  /// data being received must be valid for the lifetime of the asynchronous
  /// operation.
  void async_recv_n(void* data, size_t length, const recv_n_handler& handler,
      completion_context& context = completion_context::null())
  {
    next_layer_.async_recv_n(data, length, handler, context);
  }

private:
  /// The next layer.
  Next_Layer next_layer_;
};

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_BUFFERED_RECV_STREAM_HPP
