//
// basic_socket_connector.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#ifndef ASIO_BASIC_SOCKET_CONNECTOR_HPP
#define ASIO_BASIC_SOCKET_CONNECTOR_HPP

#include "asio/detail/push_options.hpp"

#include "asio/detail/push_options.hpp"
#include <boost/noncopyable.hpp>
#include "asio/detail/pop_options.hpp"

#include "asio/null_completion_context.hpp"
#include "asio/service_factory.hpp"

namespace asio {

/// The basic_socket_connector class template is used to connect a socket to a
/// remote endpoint. Most applications will simply use the socket_connector
/// typedef.
template <typename Service>
class basic_socket_connector
  : private boost::noncopyable
{
public:
  /// The type of the service that will be used to provide connect operations.
  typedef Service service_type;

  /// The native implementation type of the socket connector.
  typedef typename service_type::impl_type impl_type;

  /// The demuxer type for this asynchronous type.
  typedef typename service_type::demuxer_type demuxer_type;

  /// Constructor a connector. The connector is automatically opened.
  explicit basic_socket_connector(demuxer_type& d)
    : service_(d.get_service(service_factory<Service>())),
      impl_(service_type::null())
  {
    service_.create(impl_);
  }

  /// Destructor.
  ~basic_socket_connector()
  {
    service_.destroy(impl_);
  }

  /// Get the demuxer associated with the asynchronous object.
  demuxer_type& demuxer()
  {
    return service_.demuxer();
  }

  /// Open the connector.
  void open()
  {
    service_.create(impl_);
  }

  /// Close the connector.
  void close()
  {
    service_.destroy(impl_);
  }

  /// Get the underlying implementation in the native type.
  impl_type impl() const
  {
    return impl_;
  }

  /// Connect the given socket to the peer at the specified address. Throws a
  /// socket_error exception on failure.
  template <typename Stream, typename Address>
  void connect(Stream& peer_socket, const Address& peer_address)
  {
    service_.connect(impl_, peer_socket.lowest_layer(), peer_address);
  }

  /// Start an asynchronous connect. The peer_socket object must be valid until
  /// the connect's completion handler is invoked.
  template <typename Stream, typename Address, typename Handler>
  void async_connect(Stream& peer_socket, const Address& peer_address,
      Handler handler)
  {
    service_.async_connect(impl_, peer_socket.lowest_layer(), peer_address,
        handler, null_completion_context::instance());
  }

  /// Start an asynchronous connect. The peer_socket object must be valid until
  /// the connect's completion handler is invoked.
  template <typename Stream, typename Address, typename Handler,
      typename Completion_Context>
  void async_connect(Stream& peer_socket, const Address& peer_address,
      Handler handler, Completion_Context& context)
  {
    service_.async_connect(impl_, peer_socket.lowest_layer(), peer_address,
        handler, context);
  }

private:
  /// The backend service implementation.
  service_type& service_;

  /// The underlying native implementation.
  impl_type impl_;
};

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_BASIC_SOCKET_CONNECTOR_HPP
