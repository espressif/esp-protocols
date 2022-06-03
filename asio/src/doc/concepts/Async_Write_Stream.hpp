//
// Async_Write_Stream.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

/// Asynchronous write stream concept.
/**
 * @par Implemented By:
 * asio::basic_stream_socket @n
 * asio::buffered_read_stream @n
 * asio::buffered_write_stream @n
 * asio::buffered_stream
 */
class Async_Write_Stream
  : public Async_Object
{
public:
  /// Start an asynchronous write.
  /**
   * This function is used to asynchronously write data on the stream. The
   * function call always returns immediately.
   *
   * @param buffers The data to be written to the socket. Although the buffers
   * object may be copied as necessary, ownership of the underlying buffers is
   * retained by the caller, which must guarantee that they remain valid until
   * the handler is called.
   *
   * @param handler The handler to be called when the write operation completes.
   * Copies will be made of the handler as required. The equivalent function
   * signature of the handler must be:
   * @code void handler(
   *   const implementation_defined& error, // Result of operation
   *   std::size_t bytes_transferred        // Number of bytes written
   * ); @endcode
   */
  template <typename Const_Buffers, typename Handler>
  void async_write(const Const_Buffers& buffers, Handler handler);
};
