//
// write.ipp
// ~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_WRITE_IPP
#define ASIO_WRITE_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/push_options.hpp"

#include "asio/completion_condition.hpp"
#include "asio/error_handler.hpp"
#include "asio/detail/bind_handler.hpp"
#include "asio/detail/consuming_buffers.hpp"

namespace asio {

template <typename Sync_Write_Stream, typename Const_Buffers,
    typename Completion_Condition, typename Error_Handler>
std::size_t write(Sync_Write_Stream& s, const Const_Buffers& buffers,
    Completion_Condition completion_condition, Error_Handler error_handler)
{
  asio::detail::consuming_buffers<Const_Buffers> tmp(buffers);
  std::size_t total_transferred = 0;
  while (tmp.begin() != tmp.end())
  {
    typename Sync_Write_Stream::error_type e;
    std::size_t bytes_transferred = s.write_some(tmp, assign_error(e));
    tmp.consume(bytes_transferred);
    total_transferred += bytes_transferred;
    if (completion_condition(e, total_transferred))
    {
      if (e)
      {
        error_handler(e);
      }
      return total_transferred;
    }
  }
  return total_transferred;
}

template <typename Sync_Write_Stream, typename Const_Buffers>
inline std::size_t write(Sync_Write_Stream& s, const Const_Buffers& buffers)
{
  return write(s, buffers, transfer_all(), throw_error());
}

template <typename Sync_Write_Stream, typename Const_Buffers,
    typename Completion_Condition>
inline std::size_t write(Sync_Write_Stream& s, const Const_Buffers& buffers,
    Completion_Condition completion_condition)
{
  return write(s, buffers, completion_condition, throw_error());
}

namespace detail
{
  template <typename Async_Write_Stream, typename Const_Buffers,
      typename Completion_Condition, typename Handler>
  class write_handler
  {
  public:
    write_handler(Async_Write_Stream& stream, const Const_Buffers& buffers,
        Completion_Condition completion_condition, Handler handler)
      : stream_(stream),
        buffers_(buffers),
        total_transferred_(0),
        completion_condition_(completion_condition),
        handler_(handler)
    {
    }

    void operator()(const typename Async_Write_Stream::error_type& e,
        std::size_t bytes_transferred)
    {
      total_transferred_ += bytes_transferred;
      buffers_.consume(bytes_transferred);
      if (completion_condition_(e, total_transferred_)
          || buffers_.begin() == buffers_.end())
      {
        stream_.demuxer().dispatch(
            detail::bind_handler(handler_, e, total_transferred_));
      }
      else
      {
        stream_.async_write_some(buffers_, *this);
      }
    }

  private:
    Async_Write_Stream& stream_;
    asio::detail::consuming_buffers<Const_Buffers> buffers_;
    std::size_t total_transferred_;
    Completion_Condition completion_condition_;
    Handler handler_;
  };
} // namespace detail

template <typename Async_Write_Stream, typename Const_Buffers,
  typename Completion_Condition, typename Handler>
inline void async_write(Async_Write_Stream& s, const Const_Buffers& buffers,
    Completion_Condition completion_condition, Handler handler)
{
  s.async_write_some(buffers,
      detail::write_handler<Async_Write_Stream, Const_Buffers,
        Completion_Condition, Handler>(
          s, buffers, completion_condition, handler));
}

template <typename Async_Write_Stream, typename Const_Buffers, typename Handler>
inline void async_write(Async_Write_Stream& s, const Const_Buffers& buffers,
    Handler handler)
{
  async_write(s, buffers, transfer_all(), handler);
}

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_WRITE_IPP
