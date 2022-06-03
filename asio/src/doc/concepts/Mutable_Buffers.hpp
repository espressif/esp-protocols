//
// Mutable_Buffers.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2006 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

/// Mutable_Buffers concept.
/**
 * Defines the interface that must be implemented by any object passed as the
 * @c buffers parameter to functions such as:
 * @li @ref read
 * @li @ref async_read
 * @li asio::stream_socket::read_some
 * @li asio::stream_socket::async_read_some
 *
 * @par Implemented By:
 * asio::mutable_buffer_container_1 @n
 * std::deque<asio::mutable_buffer> @n
 * std::list<asio::mutable_buffer> @n
 * std::vector<asio::mutable_buffer> @n
 * boost::array<asio::mutable_buffer, N>
 */
class Mutable_Buffers
  : public CopyConstructible
{
public:
  /// The type for each element in the list of buffers. The type must be
  /// asio::mutable_buffer or be convertible to an instance of
  /// asio::mutable_buffer.
  typedef implementation_defined value_type;

  /// A forward iterator type that may be used to read elements.
  typedef implementation_defined const_iterator;

  /// Get an iterator to the first element.
  const_iterator begin() const;

  /// Get an iterator for one past the last element.
  const_iterator end() const;
};
