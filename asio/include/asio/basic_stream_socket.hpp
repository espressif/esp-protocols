//
// basic_stream_socket.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
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

#ifndef ASIO_BASIC_STREAM_SOCKET_HPP
#define ASIO_BASIC_STREAM_SOCKET_HPP

#include "asio/detail/push_options.hpp"

#include "asio/detail/push_options.hpp"
#include <boost/noncopyable.hpp>
#include "asio/detail/pop_options.hpp"

#include "asio/null_completion_context.hpp"
#include "asio/service_factory.hpp"

namespace asio {

/// The basic_stream_socket class template provides asynchronous and blocking
/// stream-oriented socket functionality. Most applications will simply use the
/// stream_socket typedef.
template <typename Service>
class basic_stream_socket
  : private boost::noncopyable
{
public:
  /// The type of the service that will be used to provide socket operations.
  typedef Service service_type;

  /// The native implementation type of the stream socket.
  typedef typename service_type::impl_type impl_type;

  /// The demuxer type for this asynchronous type.
  typedef typename service_type::demuxer_type demuxer_type;

  /// A basic_stream_socket is always the lowest layer.
  typedef basic_stream_socket<service_type> lowest_layer_type;

  /// Construct a basic_stream_socket without opening it.
  /**
   * This constructor creates a stream socket without connecting it to a remote
   * peer. The socket needs to be connected or accepted before data can be sent
   * or received on it.
   *
   * @param d The demuxer object that the stream socket will use to deliver
   * completions for any asynchronous operations performed on the socket.
   */
  explicit basic_stream_socket(demuxer_type& d)
    : service_(d.get_service(service_factory<Service>())),
      impl_(service_type::null())
  {
  }

  /// Destructor.
  ~basic_stream_socket()
  {
    service_.destroy(impl_);
  }

  /// Get the demuxer associated with the asynchronous object.
  /**
   * This function may be used to obtain the demuxer object that the stream
   * socket uses to deliver completions for asynchronous operations.
   *
   * @return A reference to the demuxer object that stream socket will use to
   * deliver completion notifications. Ownership is not transferred to the
   * caller.
   */
  demuxer_type& demuxer()
  {
    return service_.demuxer();
  }

  /// Close the socket.
  /**
   * This function is used to close the stream socket. Any asynchronous send
   * or recv operations will be immediately cancelled.
   */
  void close()
  {
    service_.destroy(impl_);
  }

  /// Get a reference to the lowest layer.
  /**
   * This function returns a reference to the lowest layer in a stack of
   * stream layers. Since a basic_stream_socket cannot contain any further
   * stream layers, it simply returns a reference to itself.
   *
   * @return A reference to the lowest layer in the stack of stream layers.
   * Ownership is not transferred to the caller.
   */
  lowest_layer_type& lowest_layer()
  {
    return *this;
  }

  /// Get the underlying implementation in the native type.
  /**
   * This function may be used to obtain the underlying implementation of the
   * stream socket. This is intended to allow access to native socket
   * functionality that is not otherwise provided.
   */
  impl_type impl()
  {
    return impl_;
  }

  /// Set the underlying implementation in the native type.
  /**
   * This function is used by the acceptor and connector implementations to set
   * the underlying implementation associated with the stream socket.
   *
   * @param new_impl The new underlying socket implementation.
   */
  void set_impl(impl_type new_impl)
  {
    service_.create(impl_, new_impl);
  }

  /// Send the given data to the peer.
  /**
   * This function is used to send data to the stream socket's peer. The
   * function call will block until the data has been sent successfully or an
   * error occurs.
   *
   * @param data The data to be sent to remote peer.
   *
   * @param length The size of the data to be sent, in bytes.
   *
   * @returns The number of bytes sent or 0 if the connection was closed
   * cleanly.
   *
   * @throws socket_error Thrown on failure.
   */
  size_t send(const void* data, size_t length)
  {
    return service_.send(impl_, data, length);
  }

  /// Start an asynchronous send.
  /**
   * This function is used to asynchronously send data to the stream socket's
   * peer. The function call always returns immediately.
   *
   * @param data The data to be sent to the remote peer. Ownership of the data
   * is retained by the caller, which must guarantee that it is valid until the
   * handler is called.
   *
   * @param length The size of the data to be sent, in bytes.
   *
   * @param handler The completion handler to be called when the send operation
   * completes. Copies will be made of the handler as required. The equivalent
   * function signature of the handler must be:
   * @code void handler(
   *   const asio::socket_error& error, // Result of operation
   *   size_t bytes_sent                // Number of bytes sent
   * ); @endcode
   */
  template <typename Handler>
  void async_send(const void* data, size_t length, Handler handler)
  {
    service_.async_send(impl_, data, length, handler,
        null_completion_context::instance());
  }

  /// Start an asynchronous send.
  /**
   * This function is used to asynchronously send data to the stream socket's
   * peer. The function call always returns immediately.
   *
   * @param data The data to be sent to the remote peer. Ownership of the data
   * is retained by the caller, which must guarantee that it is valid until the
   * handler is called.
   *
   * @param length The size of the data to be sent, in bytes.
   *
   * @param handler The completion handler to be called when the send operation
   * completes. Copies will be made of the handler as required. The equivalent
   * function signature of the handler must be:
   * @code void handler(
   *   const asio::socket_error& error, // Result of operation
   *   size_t bytes_sent                // Number of bytes sent
   * ); @endcode
   *
   * @param context The completion context which controls the number of
   * concurrent invocations of handlers that may be made. Ownership of the
   * object is retained by the caller, which must guarantee that it is valid
   * until after the handler has been called.
   */
  template <typename Handler, typename Completion_Context>
  void async_send(const void* data, size_t length, Handler handler,
      Completion_Context& context)
  {
    service_.async_send(impl_, data, length, handler, context);
  }

  /// Receive some data from the peer.
  /**
   * This function is used to receive data from the stream socket's peer. The
   * function call will block until data has received successfully or an error
   * occurs.
   *
   * @param data The buffer into which the received data will be written.
   *
   * @param max_length The maximum size of the data to be received, in bytes.
   *
   * @returns The number of bytes received or 0 if the connection was closed
   * cleanly.
   *
   * @throws socket_error Thrown on failure.
   */
  size_t recv(void* data, size_t max_length)
  {
    return service_.recv(impl_, data, max_length);
  }

  /// Start an asynchronous receive.
  /**
   * This function is used to asynchronously receive data from the stream
   * socket's peer. The function call always returns immediately.
   *
   * @param data The buffer into which the received data will be written.
   * Ownership of the buffer is retained by the caller, which must guarantee
   * that it is valid until the handler is called.
   *
   * @param max_length The maximum size of the data to be received, in bytes.
   *
   * @param handler The completion handler to be called when the receive
   * operation completes. Copies will be made of the handler as required. The
   * equivalent function signature of the handler must be:
   * @code void handler(
   *   const asio::socket_error& error, // Result of operation
   *   size_t bytes_received            // Number of bytes received
   * ); @endcode
   */
  template <typename Handler>
  void async_recv(void* data, size_t max_length, Handler handler)
  {
    service_.async_recv(impl_, data, max_length, handler,
        null_completion_context::instance());
  }

  /// Start an asynchronous receive.
  /**
   * This function is used to asynchronously receive data from the stream
   * socket's peer. The function call always returns immediately.
   *
   * @param data The buffer into which the received data will be written.
   * Ownership of the buffer is retained by the caller, which must guarantee
   * that it is valid until the handler is called.
   *
   * @param max_length The maximum size of the data to be received, in bytes.
   *
   * @param handler The completion handler to be called when the receive
   * operation completes. Copies will be made of the handler as required. The
   * equivalent function signature of the handler must be:
   * @code void handler(
   *   const asio::socket_error& error, // Result of operation
   *   size_t bytes_received            // Number of bytes received
   * ); @endcode
   *
   * @param context The completion context which controls the number of
   * concurrent invocations of handlers that may be made. Ownership of the
   * object is retained by the caller, which must guarantee that it is valid
   * until after the handler has been called.
   */
  template <typename Handler, typename Completion_Context>
  void async_recv(void* data, size_t max_length, Handler handler,
      Completion_Context& context)
  {
    service_.async_recv(impl_, data, max_length, handler, context);
  }

private:
  /// The backend service implementation.
  service_type& service_;

  /// The underlying native implementation.
  impl_type impl_;
};

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_BASIC_STREAM_SOCKET_HPP
