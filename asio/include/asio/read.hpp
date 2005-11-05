//
// read.hpp
// ~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_READ_HPP
#define ASIO_READ_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

#include "asio/detail/push_options.hpp"
#include <cstddef>
#include <boost/config.hpp>
#include "asio/detail/pop_options.hpp"

#include "asio/detail/bind_handler.hpp"
#include "asio/detail/consuming_buffers.hpp"

namespace asio {

/**
 * @defgroup read asio::read
 */
/*@{*/

/// Read some data from a stream.
/**
 * This function is used to read data from a stream. The function call will
 * block until data has been read successfully or an error occurs.
 *
 * @param s The stream from which the data is to be read. The type must support
 * the Sync_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read.
 *
 * @returns The number of bytes read, or 0 if the stream was closed cleanly.
 *
 * @note Throws an exception on failure. The type of the exception depends
 * on the underlying stream's read operation.
 *
 * @note The read operation may not read all of the requested number of bytes.
 * Consider using the \ref read_n function if you need to ensure that the
 * requested amount of data is read before the blocking operation completes.
 *
 * @par Example:
 * To read into a single data buffer use the @ref buffer function as follows:
 * @code asio::read(s, asio::buffer(data, size)); @endcode
 * See the @ref buffer documentation for information on reading into multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Sync_Read_Stream, typename Mutable_Buffers>
inline std::size_t read(Sync_Read_Stream& s, const Mutable_Buffers& buffers)
{
  return s.read(buffers);
}

/// Read some data from a stream.
/**
 * This function is used to read data from a stream. The function call will
 * block until data has been read successfully or an error occurs.
 *
 * @param s The stream from which the data is to be read. The type must support
 * the Sync_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read.
 *
 * @param error_handler The handler to be called when an error occurs. Copies
 * will be made of the handler as required. The equivalent function signature
 * of the handler must be:
 * @code template <typename Error>
 * void error_handler(
 *   const Error& error // Result of operation (the actual type is dependent on
 *                      // the underlying stream's read operation).
 * ); @endcode
 *
 * @returns The number of bytes read, or 0 if the stream was closed cleanly.
 *
 * @note The read operation may not read all of the requested number of bytes.
 * Consider using the \ref read_n function if you need to ensure that the
 * requested amount of data is read before the blocking operation completes.
 */
template <typename Sync_Read_Stream, typename Mutable_Buffers,
    typename Error_Handler>
inline std::size_t read(Sync_Read_Stream& s, const Mutable_Buffers& buffers,
    Error_Handler error_handler)
{
  return s.read(buffers, error_handler);
}

/*@}*/
/**
 * @defgroup async_read asio::async_read
 */
/*@{*/

/// Start an asynchronous read.
/**
 * This function is used to asynchronously read data from a stream. The function
 * call always returns immediately.
 *
 * @param s The stream from which the data is to be read. The type must support
 * the Async_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the handler is called.
 *
 * @param handler The handler to be called when the read operation completes.
 * Copies will be made of the handler as required. The equivalent function
 * signature of the handler must be:
 * @code template <typename Error>
 * void handler(
 *   const Error& error,           // Result of operation (the actual type is
 *                                 // dependent on the underlying stream's read
 *                                 // operation).
 *
 *   std::size_t bytes_transferred // Number of bytes read, or 0 if the stream
 *                                 // was closed cleanly.
 * ); @endcode
 *
 * @note The read operation may not read all of the requested number of bytes.
 * Consider using the \ref async_read_n function if you need to ensure that the
 * requested amount of data is read before the asynchronous operation
 * completes.
 *
 * @par Example:
 * To read into a single data buffer use the @ref buffer function as follows:
 * @code asio::async_read(s, asio::buffer(data, size), handler); @endcode
 * See the @ref buffer documentation for information on reading into multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Async_Read_Stream, typename Mutable_Buffers,
    typename Handler>
inline void async_read(Async_Read_Stream& s, const Mutable_Buffers& buffers,
    Handler handler)
{
  s.async_read(buffers, handler);
}

/*@}*/
/**
 * @defgroup read_n asio::read_n
 */
/*@{*/

/// Attempt to read a certain amount of data from a stream before returning.
/**
 * This function is used to read a certain number of bytes of data from a
 * stream. The call will block until one of the following conditions is true:
 *
 * @li The supplied buffers are full. That is, the total bytes transferred is
 * equal to the sum of the buffer sizes.
 *
 * @li The stream was closed cleanly.
 *
 * @li An error occurred.
 *
 * This function is implemented in terms of one or more calls to \ref read.
 * 
 * @param s The stream from which the data is to be read. The type must support
 * the Sync_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read. The sum
 * of the buffer sizes indicates the number of bytes to read from the stream.
 *
 * @param total_bytes_transferred An optional output parameter that receives the
 * total number of bytes copied into the buffers. If the stream was closed, or
 * an error occurred, this will be less than the sum of the buffer sizes.
 *
 * @returns The number of bytes transferred on the last read, or 0 if the
 * stream was closed cleanly.
 *
 * @note Throws an exception on failure. The type of the exception depends
 * on the underlying stream's read operation.
 *
 * @par Example:
 * To read into a single data buffer use the @ref buffer function as follows:
 * @code asio::read_n(s, asio::buffer(data, size)); @endcode
 * See the @ref buffer documentation for information on reading into multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Sync_Read_Stream, typename Mutable_Buffers>
std::size_t read_n(Sync_Read_Stream& s, const Mutable_Buffers& buffers,
    std::size_t* total_bytes_transferred = 0)
{
  asio::detail::consuming_buffers<Mutable_Buffers> tmp(buffers);
  std::size_t bytes_transferred = 0;
  std::size_t total_transferred = 0;
  while (tmp.begin() != tmp.end())
  {
    bytes_transferred = read(s, tmp);
    if (bytes_transferred == 0)
    {
      if (total_bytes_transferred)
        *total_bytes_transferred = total_transferred;
      return bytes_transferred;
    }
    tmp.consume(bytes_transferred);
    total_transferred += bytes_transferred;
  }
  if (total_bytes_transferred)
    *total_bytes_transferred = total_transferred;
  return bytes_transferred;
}

/// Attempt to read a certain amount of data from a stream before returning.
/**
 * This function is used to read a certain number of bytes of data from a
 * stream. The call will block until one of the following conditions is true:
 *
 * @li The supplied buffers are full. That is, the total bytes transferred is
 * equal to the sum of the buffer sizes.
 *
 * @li The stream was closed cleanly.
 *
 * @li An error occurred.
 *
 * This function is implemented in terms of one or more calls to \ref read.
 * 
 * @param s The stream from which the data is to be read. The type must support
 * the Sync_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read. The sum
 * of the buffer sizes indicates the number of bytes to read from the stream.
 *
 * @param total_bytes_transferred An optional output parameter that receives the
 * total number of bytes copied into the buffers. If the stream was closed, or
 * an error occurred, this will be less than the sum of the buffer sizes.
 *
 * @param error_handler The handler to be called when an error occurs. Copies
 * will be made of the handler as required. The equivalent function signature
 * of the handler must be:
 * @code template <typename Error>
 * void error_handler(
 *   const Error& error // Result of operation (the actual type is dependent on
 *                      // the underlying stream's read operation).
 * ); @endcode
 *
 * @returns The number of bytes transferred on the last read, or 0 if the
 * stream was closed cleanly.
 */
template <typename Sync_Read_Stream, typename Mutable_Buffers,
    typename Error_Handler>
std::size_t read_n(Sync_Read_Stream& s, const Mutable_Buffers& buffers,
    std::size_t* total_bytes_transferred, Error_Handler error_handler)
{
  asio::detail::consuming_buffers<Mutable_Buffers> tmp(buffers);
  std::size_t bytes_transferred = 0;
  std::size_t total_transferred = 0;
  while (tmp.begin() != tmp.end())
  {
    bytes_transferred = read(s, tmp, error_handler);
    if (bytes_transferred == 0)
    {
      if (total_bytes_transferred)
        *total_bytes_transferred = total_transferred;
      return bytes_transferred;
    }
    tmp.consume(bytes_transferred);
    total_transferred += bytes_transferred;
  }
  if (total_bytes_transferred)
    *total_bytes_transferred = total_transferred;
  return bytes_transferred;
}

/*@}*/

namespace detail
{
  template <typename Async_Read_Stream, typename Mutable_Buffers,
      typename Handler>
  class read_n_handler
  {
  public:
    read_n_handler(Async_Read_Stream& stream, const Mutable_Buffers& buffers,
        Handler handler)
      : stream_(stream),
        buffers_(buffers),
        total_transferred_(0),
        handler_(handler)
    {
    }

    template <typename Error>
    void operator()(const Error& e, std::size_t bytes_transferred)
    {
      total_transferred_ += bytes_transferred;
      buffers_.consume(bytes_transferred);
      if (e || bytes_transferred == 0 || buffers_.begin() == buffers_.end())
      {
        stream_.demuxer().dispatch(detail::bind_handler(handler_, e,
              bytes_transferred, total_transferred_));
      }
      else
      {
        asio::async_read(stream_, buffers_, *this);
      }
    }

  private:
    Async_Read_Stream& stream_;
    asio::detail::consuming_buffers<Mutable_Buffers> buffers_;
    std::size_t total_transferred_;
    Handler handler_;
  };
} // namespace detail

/**
 * @defgroup async_read_n asio::async_read_n
 */
/*@{*/

/// Start an asynchronous attempt to read a certain amount of data from a
/// stream.
/**
 * This function is used to asynchronously read a certain number of bytes of
 * data from a stream. The function call always returns immediately. The
 * asynchronous operation will continue until one of the following conditions is
 * true:
 *
 * @li The supplied buffers are full. That is, the total bytes transferred is
 * equal to the sum of the buffer sizes.
 *
 * @li The stream was closed cleanly.
 *
 * @li An error occurred.
 *
 * This asynchronous operation is implemented in terms of one or more calls to
 * \ref async_read.
 * 
 * @param s The stream from which the data is to be read. The type must support
 * the Async_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read. The sum
 * of the buffer sizes indicates the number of bytes to read from the stream.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the handler is called.
 *
 * @param handler The handler to be called when the read operation completes.
 * Copies will be made of the handler as required. The equivalent function
 * signature of the handler must be:
 * @code template <typename Error>
 * void handler(
 *   const Error& error,                 // Result of operation (the actual type
 *                                       // is dependent on the underlying
 *                                       // stream's read operation).
 *
 *   std::size_t last_bytes_transferred, // Number of bytes transferred on last
 *                                       // read, or 0 if the stream was closed
 *                                       // cleanly.
 *
 *   std::size_t total_bytes_transferred // Total number of bytes copied into
 *                                       // the buffers. If the stream was
 *                                       // closed, or an error occurred, this
 *                                       // will be less than the sum of the
 *                                       // buffer sizes.
 * ); @endcode
 *
 * @par Example:
 * To read into a single data buffer use the @ref buffer function as follows:
 * @code asio::async_read_n(s, asio::buffer(data, size), handler); @endcode
 * See the @ref buffer documentation for information on reading into multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Async_Read_Stream, typename Mutable_Buffers,
    typename Handler>
inline void async_read_n(Async_Read_Stream& s, const Mutable_Buffers& buffers,
    Handler handler)
{
  async_read(s, buffers,
      detail::read_n_handler<Async_Read_Stream, Mutable_Buffers, Handler>(
        s, buffers, handler));
}

/*@}*/
/**
 * @defgroup read_at_least_n asio::read_at_least_n
 */
/*@{*/

/// Attempt to read at least a certain amount of data from a stream before
/// returning.
/**
 * This function is used to read at least a certain number of bytes of data from
 * a stream. The call will block until one of the following conditions is true:
 *
 * @li The total bytes transferred is greater than or equal to the specified
 * minimum size.
 *
 * @li The supplied buffers are full. That is, the total bytes transferred is
 * equal to the sum of the buffer sizes.
 *
 * @li The stream was closed cleanly.
 *
 * @li An error occurred.
 *
 * This function is implemented in terms of one or more calls to \ref read.
 * 
 * @param s The stream from which the data is to be read. The type must support
 * the Sync_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read. The sum
 * of the buffer sizes indicates the maximum number of bytes to read from the
 * stream.
 *
 * @param min_length The minimum size of the data to be read, in bytes.
 *
 * @param total_bytes_transferred An optional output parameter that receives the
 * total number of bytes copied into the buffers. If the stream was closed, or
 * an error occurred, this will be less than the minimum size.
 *
 * @returns The number of bytes transferred on the last read, or 0 if the
 * stream was closed cleanly.
 *
 * @note Throws an exception on failure. The type of the exception depends
 * on the underlying stream's read operation.
 *
 * @par Example:
 * To read into a single data buffer use the @ref buffer function as follows:
 * @code asio::read_at_least_n(s, asio::buffer(data, size), min_length);
 * @endcode
 * See the @ref buffer documentation for information on reading into multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Sync_Read_Stream, typename Mutable_Buffers>
std::size_t read_at_least_n(Sync_Read_Stream& s, const Mutable_Buffers& buffers,
    std::size_t min_length, std::size_t* total_bytes_transferred = 0)
{
  asio::detail::consuming_buffers<Mutable_Buffers> tmp(buffers);
  std::size_t bytes_transferred = 0;
  std::size_t total_transferred = 0;
  while (tmp.begin() != tmp.end() && total_transferred < min_length)
  {
    bytes_transferred = read(s, tmp);
    if (bytes_transferred == 0)
    {
      if (total_bytes_transferred)
        *total_bytes_transferred = total_transferred;
      return bytes_transferred;
    }
    tmp.consume(bytes_transferred);
    total_transferred += bytes_transferred;
  }
  if (total_bytes_transferred)
    *total_bytes_transferred = total_transferred;
  return bytes_transferred;
}

/// Attempt to read at least a certain amount of data from a stream before
/// returning.
/**
 * This function is used to read at least a certain number of bytes of data from
 * a stream. The call will block until one of the following conditions is true:
 *
 * @li The total bytes transferred is greater than or equal to the specified
 * minimum size.
 *
 * @li The supplied buffers are full. That is, the total bytes transferred is
 * equal to the sum of the buffer sizes.
 *
 * @li The stream was closed cleanly.
 *
 * @li An error occurred.
 *
 * This function is implemented in terms of one or more calls to \ref read.
 * 
 * @param s The stream from which the data is to be read. The type must support
 * the Sync_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read. The sum
 * of the buffer sizes indicates the maximum number of bytes to read from the
 * stream.
 *
 * @param min_length The minimum size of the data to be read, in bytes.
 *
 * @param total_bytes_transferred An optional output parameter that receives the
 * total number of bytes copied into the buffers. If the stream was closed, or
 * an error occurred, this will be less than the minimum size.
 *
 * @param error_handler The handler to be called when an error occurs. Copies
 * will be made of the handler as required. The equivalent function signature
 * of the handler must be:
 * @code template <typename Error>
 * void error_handler(
 *   const Error& error // Result of operation (the actual type is dependent on
 *                      // the underlying stream's read operation).
 * ); @endcode
 *
 * @returns The number of bytes transferred on the last read, or 0 if the
 * stream was closed cleanly.
 */
template <typename Sync_Read_Stream, typename Mutable_Buffers,
    typename Error_Handler>
std::size_t read_at_least_n(Sync_Read_Stream& s, const Mutable_Buffers& buffers,
    std::size_t min_length, std::size_t* total_bytes_transferred,
    Error_Handler error_handler)
{
  asio::detail::consuming_buffers<Mutable_Buffers> tmp(buffers);
  std::size_t bytes_transferred = 0;
  std::size_t total_transferred = 0;
  while (tmp.begin() != tmp.end() && total_transferred < min_length)
  {
    bytes_transferred = read(s, tmp, error_handler);
    if (bytes_transferred == 0)
    {
      if (total_bytes_transferred)
        *total_bytes_transferred = total_transferred;
      return bytes_transferred;
    }
    tmp.consume(bytes_transferred);
    total_transferred += bytes_transferred;
  }
  if (total_bytes_transferred)
    *total_bytes_transferred = total_transferred;
  return bytes_transferred;
}

/*@}*/

namespace detail
{
  template <typename Async_Read_Stream, typename Mutable_Buffers,
      typename Handler>
  class read_at_least_n_handler
  {
  public:
    read_at_least_n_handler(Async_Read_Stream& stream,
        const Mutable_Buffers& buffers, std::size_t min_length, Handler handler)
      : stream_(stream),
        buffers_(buffers),
        min_length_(min_length),
        total_transferred_(0),
        handler_(handler)
    {
    }

    template <typename Error>
    void operator()(const Error& e, std::size_t bytes_transferred)
    {
      total_transferred_ += bytes_transferred;
      buffers_.consume(bytes_transferred);
      if (e || bytes_transferred == 0 || buffers_.begin() == buffers_.end()
          || total_transferred_ >= min_length_)
      {
        stream_.demuxer().dispatch(detail::bind_handler(handler_, e,
              bytes_transferred, total_transferred_));
      }
      else
      {
        asio::async_read(stream_, buffers_, *this);
      }
    }

  private:
    Async_Read_Stream& stream_;
    asio::detail::consuming_buffers<Mutable_Buffers> buffers_;
    std::size_t min_length_;
    std::size_t total_transferred_;
    Handler handler_;
  };
} // namespace detail

/**
 * @defgroup async_read_at_least_n asio::async_read_at_least_n
 */
/*@{*/

/// Start an asynchronous attempt to read at least a certain amount of data from
/// a stream.
/**
 * This function is used to asynchronously read at least a certain number of
 * bytes of data from a stream. The function call always returns immediately.
 * The asynchronous operation will continue until one of the following
 * conditions is true:
 *
 * @li The total bytes transferred is greater than or equal to the specified
 * minimum size.
 *
 * @li The supplied buffers are full. That is, the total bytes transferred is
 * equal to the sum of the buffer sizes.
 *
 * @li The stream was closed cleanly.
 *
 * @li An error occurred.
 *
 * This asynchronous operation is implemented in terms of one or more calls to
 * \ref async_read.
 * 
 * @param s The stream from which the data is to be read. The type must support
 * the Async_Read_Stream concept.
 *
 * @param buffers One or more buffers into which the data will be read. The sum
 * of the buffer sizes indicates the number of bytes to read from the stream.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the handler is called.
 *
 * @param min_length The minimum size of the data to be read, in bytes.
 *
 * @param handler The handler to be called when the read operation completes.
 * Copies will be made of the handler as required. The equivalent function
 * signature of the handler must be:
 * @code template <typename Error>
 * void handler(
 *   const Error& error,                 // Result of operation (the actual type
 *                                       // is dependent on the underlying
 *                                       // stream's read operation).
 *
 *   std::size_t last_bytes_transferred, // Number of bytes transferred on last
 *                                       // read, or 0 if the stream was closed
 *                                       // cleanly.
 *
 *   std::size_t total_bytes_transferred // Total number of bytes copied into
 *                                       // the buffers. If the stream was
 *                                       // closed, or an error occurred, this
 *                                       // will be less than the minimum size.
 * ); @endcode
 *
 * @par Example:
 * To read into a single data buffer use the @ref buffer function as follows:
 * @code asio::async_read_at_least_n(s,
 *     asio::buffer(data, size), min_length, handler); @endcode
 * See the @ref buffer documentation for information on reading into multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Async_Read_Stream, typename Mutable_Buffers,
    typename Handler>
inline void async_read_at_least_n(Async_Read_Stream& s,
    const Mutable_Buffers& buffers, std::size_t min_length, Handler handler)
{
  async_read(s, buffers,
      detail::read_at_least_n_handler<Async_Read_Stream, Mutable_Buffers,
          Handler>(s, buffers, min_length, handler));
}

/*@}*/

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_READ_HPP
