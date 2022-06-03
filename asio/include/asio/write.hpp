//
// write.hpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_WRITE_HPP
#define ASIO_WRITE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

#include "asio/detail/push_options.hpp"
#include <cstddef>
#include <boost/config.hpp>
#include "asio/detail/pop_options.hpp"

namespace asio {

/**
 * @defgroup write asio::write
 */
/*@{*/

/// Write all of the supplied data to a stream before returning.
/**
 * This function is used to write a certain number of bytes of data to a stream.
 * The call will block until one of the following conditions is true:
 *
 * @li All of the data in the supplied buffers has been written. That is, the
 * bytes transferred is equal to the sum of the buffer sizes.
 *
 * @li An error occurred.
 *
 * This operation is implemented in terms of one or more calls to the stream's
 * write_some function.
 *
 * @param s The stream to which the data is to be written. The type must support
 * the Sync_Write_Stream concept.
 *
 * @param buffers One or more buffers containing the data to be written. The sum
 * of the buffer sizes indicates the maximum number of bytes to write to the
 * stream.
 *
 * @returns The number of bytes transferred.
 *
 * @throws Sync_Write_Stream::error_type Thrown on failure.
 *
 * @par Example:
 * To write a single data buffer use the @ref buffer function as follows:
 * @code asio::write(s, asio::buffer(data, size)); @endcode
 * See the @ref buffer documentation for information on writing multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 *
 * @note This overload is equivalent to calling:
 * @code asio::write(
 *     s, buffers,
 *     asio::transfer_all(),
 *     asio::throw_error()); @endcode
 */
template <typename Sync_Write_Stream, typename Const_Buffers>
std::size_t write(Sync_Write_Stream& s, const Const_Buffers& buffers);

/// Write a certain amount of data to a stream before returning.
/**
 * This function is used to write a certain number of bytes of data to a stream.
 * The call will block until one of the following conditions is true:
 *
 * @li All of the data in the supplied buffers has been written. That is, the
 * bytes transferred is equal to the sum of the buffer sizes.
 *
 * @li The completion_condition function object returns true.
 *
 * This operation is implemented in terms of one or more calls to the stream's
 * write_some function.
 *
 * @param s The stream to which the data is to be written. The type must support
 * the Sync_Write_Stream concept.
 *
 * @param buffers One or more buffers containing the data to be written. The sum
 * of the buffer sizes indicates the maximum number of bytes to write to the
 * stream.
 *
 * @param completion_condition The function object to be called to determine
 * whether the write operation is complete. The equivalent function signature
 * of the handler must be:
 * @code bool completion_condition(
 *   const Sync_Write_Stream::error_type& error, // Result of latest write_some
 *                                               // operation.
 *
 *   std::size_t bytes_transferred               // Number of bytes transferred
 *                                               // so far.
 * ); @endcode
 * A return value of true indicates that the write operation is complete. False
 * indicates that further calls to the stream's write_some function are
 * required.
 *
 * @returns The number of bytes transferred.
 *
 * @throws Sync_Write_Stream::error_type Thrown on failure.
 *
 * @par Example:
 * To write a single data buffer use the @ref buffer function as follows:
 * @code asio::write(s, asio::buffer(data, size),
 *     asio::transfer_at_least(32)); @endcode
 * See the @ref buffer documentation for information on writing multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 *
 * @note This overload is equivalent to calling:
 * @code asio::write(
 *     s, buffers,
 *     completion_condition,
 *     asio::throw_error()); @endcode
 */
template <typename Sync_Write_Stream, typename Const_Buffers,
    typename Completion_Condition>
std::size_t write(Sync_Write_Stream& s, const Const_Buffers& buffers,
    Completion_Condition completion_condition);

/// Write a certain amount of data to a stream before returning.
/**
 * This function is used to write a certain number of bytes of data to a stream.
 * The call will block until one of the following conditions is true:
 *
 * @li All of the data in the supplied buffers has been written. That is, the
 * bytes transferred is equal to the sum of the buffer sizes.
 *
 * @li The completion_condition function object returns true.
 *
 * This operation is implemented in terms of one or more calls to the stream's
 * write_some function.
 *
 * @param s The stream to which the data is to be written. The type must support
 * the Sync_Write_Stream concept.
 *
 * @param buffers One or more buffers containing the data to be written. The sum
 * of the buffer sizes indicates the maximum number of bytes to write to the
 * stream.
 *
 * @param completion_condition The function object to be called to determine
 * whether the write operation is complete. The equivalent function signature
 * of the handler must be:
 * @code bool completion_condition(
 *   const Sync_Write_Stream::error_type& error, // Result of latest write_some
 *                                               // operation.
 *
 *   std::size_t bytes_transferred               // Number of bytes transferred
 *                                               // so far.
 * ); @endcode
 * A return value of true indicates that the write operation is complete. False
 * indicates that further calls to the stream's write_some function are
 * required.
 *
 * @param error_handler The handler to be called when an error occurs. Copies
 * will be made of the handler as required. The equivalent function signature
 * of the handler must be:
 * @code void error_handler(
 *   const Sync_Write_Stream::error_type& error // Result of operation.
 * ); @endcode
 *
 * @returns The number of bytes written. If an error occurs, and the error
 * handler does not throw an exception, returns the total number of bytes
 * successfully transferred prior to the error.
 */
template <typename Sync_Write_Stream, typename Const_Buffers,
    typename Completion_Condition, typename Error_Handler>
std::size_t write(Sync_Write_Stream& s, const Const_Buffers& buffers,
    Completion_Condition completion_condition, Error_Handler error_handler);

/*@}*/
/**
 * @defgroup async_write asio::async_write
 */
/*@{*/

/// Start an asynchronous operation to write of all of the supplied data to a
/// stream.
/**
 * This function is used to asynchronously write a certain number of bytes of
 * data to a stream. The function call always returns immediately. The
 * asynchronous operation will continue until one of the following conditions
 * is true:
 *
 * @li All of the data in the supplied buffers has been written. That is, the
 * bytes transferred is equal to the sum of the buffer sizes.
 *
 * @li An error occurred.
 *
 * This operation is implemented in terms of one or more calls to the stream's
 * async_write_some function.
 *
 * @param s The stream to which the data is to be written. The type must support
 * the Async_Write_Stream concept.
 *
 * @param buffers One or more buffers containing the data to be written.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the handler is called.
 *
 * @param handler The handler to be called when the write operation completes.
 * Copies will be made of the handler as required. The equivalent function
 * signature of the handler must be:
 * @code void handler(
 *   const Async_Write_Stream::error_type& error, // Result of operation.
 *
 *   std::size_t bytes_transferred                // Number of bytes written
 *                                                // from the buffers. If an
 *                                                // error occurred, this will
 *                                                // be less than the sum of the
 *                                                // buffer sizes.
 * ); @endcode
 *
 * @par Example:
 * To write a single data buffer use the @ref buffer function as follows:
 * @code
 * asio::async_write(s, asio::buffer(data, size), handler);
 * @endcode
 * See the @ref buffer documentation for information on writing multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Async_Write_Stream, typename Const_Buffers, typename Handler>
void async_write(Async_Write_Stream& s, const Const_Buffers& buffers,
    Handler handler);

/// Start an asynchronous operation to write a certain amount of data to a
/// stream.
/**
 * This function is used to asynchronously write a certain number of bytes of
 * data to a stream. The function call always returns immediately. The
 * asynchronous operation will continue until one of the following conditions
 * is true:
 *
 * @li All of the data in the supplied buffers has been written. That is, the
 * bytes transferred is equal to the sum of the buffer sizes.
 *
 * @li The completion_condition function object returns true.
 *
 * This operation is implemented in terms of one or more calls to the stream's
 * async_write_some function.
 *
 * @param s The stream to which the data is to be written. The type must support
 * the Async_Write_Stream concept.
 *
 * @param buffers One or more buffers containing the data to be written.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the handler is called.
 *
 * @param completion_condition The function object to be called to determine
 * whether the write operation is complete. The equivalent function signature
 * of the handler must be:
 * @code bool completion_condition(
 *   const Async_Write_Stream::error_type& error, // Result of latest write_some
 *                                                // operation.
 *
 *   std::size_t bytes_transferred                // Number of bytes transferred
 *                                                // so far.
 * ); @endcode
 * A return value of true indicates that the write operation is complete. False
 * indicates that further calls to the stream's async_write_some function are
 * required.
 *
 * @param handler The handler to be called when the write operation completes.
 * Copies will be made of the handler as required. The equivalent function
 * signature of the handler must be:
 * @code void handler(
 *   const Async_Write_Stream::error_type& error, // Result of operation.
 *
 *   std::size_t bytes_transferred                // Number of bytes written
 *                                                // from the buffers. If an
 *                                                // error occurred, this will
 *                                                // be less than the sum of the
 *                                                // buffer sizes.
 * ); @endcode
 *
 * @par Example:
 * To write a single data buffer use the @ref buffer function as follows:
 * @code asio::async_write(s,
 *     asio::buffer(data, size),
 *     asio::transfer_at_least(32),
 *     handler); @endcode
 * See the @ref buffer documentation for information on writing multiple
 * buffers in one go, and how to use it with arrays, boost::array or
 * std::vector.
 */
template <typename Async_Write_Stream, typename Const_Buffers,
    typename Completion_Condition, typename Handler>
void async_write(Async_Write_Stream& s, const Const_Buffers& buffers,
    Completion_Condition completion_condition, Handler handler);

/*@}*/

} // namespace asio

#include "asio/write.ipp"

#include "asio/detail/pop_options.hpp"

#endif // ASIO_WRITE_HPP
